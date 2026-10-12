#include "Wallet.hpp"

#include <algorithm>
#include <cerrno>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <system_error>
#include <unordered_map>

#include "Config.hpp"

namespace llm_npc {

namespace {

// A whole string that parses as a finite, non-negative number. stod alone
// throws on "abc" and accepts "nan" and "-1", none of which is a price.
std::optional<double> parseAmount(const std::string& text) {
    const char* begin = text.c_str();
    char* end = nullptr;
    errno = 0;
    const double value = std::strtod(begin, &end);
    if (end == begin || *end != '\0' || errno == ERANGE) return std::nullopt;
    if (!std::isfinite(value) || value < 0.0) return std::nullopt;
    return value;
}

// A whole string that parses as an int64; "", "1.5" and overflow are rejected.
std::optional<std::int64_t> parseMicros(const std::string& text) {
    std::int64_t value = 0;
    const char* last = text.data() + text.size();
    const auto [ptr, ec] = std::from_chars(text.data(), last, value);
    if (ec != std::errc() || ptr != last || text.empty()) return std::nullopt;
    return value;
}

// Overwrites `field` with kv[key] when present and valid; logs and keeps the
// default otherwise, so a typo in llm.cfg cannot stop the game from starting.
void readAmount(const std::unordered_map<std::string, std::string>& kv, const char* key,
                double& field) {
    const auto it = kv.find(key);
    if (it == kv.end()) return;
    if (const auto value = parseAmount(it->second)) {
        field = *value;
    } else {
        std::cerr << "[llm_npc] wallet: ignoring " << key << " = \"" << it->second
                  << "\" (expected a non-negative number)\n";
    }
}

// Largest starter credit accepted, so llround cannot overflow: $1M.
constexpr double kMaxStarterUsd = 1'000'000.0;

}  // namespace

WalletSettings loadWalletSettings(const std::filesystem::path& configDir) {
    WalletSettings settings;
    CloudPricing& p = settings.pricing;
    const auto kv = readKv(configDir / "llm.cfg");
    readAmount(kv, "price_input_per_mtok", p.inputPerMTok);
    readAmount(kv, "price_output_per_mtok", p.outputPerMTok);
    readAmount(kv, "price_cache_write_per_mtok", p.cacheWritePerMTok);
    readAmount(kv, "price_cache_read_per_mtok", p.cacheReadPerMTok);
    readAmount(kv, "markup_percent", p.markupPercent);
    double starterUsd = static_cast<double>(settings.starterCreditMicros) / kMicrosPerUsd;
    readAmount(kv, "starter_credit_usd", starterUsd);
    settings.starterCreditMicros =
        std::llround(std::min(starterUsd, kMaxStarterUsd) * kMicrosPerUsd);
    return settings;
}

double costMicros(const AnthropicUsage& usage, const CloudPricing& pricing) {
    return static_cast<double>(usage.inputTokens) * pricing.inputPerMTok +
           static_cast<double>(usage.outputTokens) * pricing.outputPerMTok +
           static_cast<double>(usage.cacheWriteTokens) * pricing.cacheWritePerMTok +
           static_cast<double>(usage.cacheReadTokens) * pricing.cacheReadPerMTok;
}

std::int64_t priceMicros(const AnthropicUsage& usage, const CloudPricing& pricing) {
    // Cost x (1 + markup), rounded up so a charge never undercuts cost. The
    // tolerance stops float noise (100 * 1.1 = 110.00000000000001) from
    // bumping an exact amount up by a micro-dollar.
    const double raw = costMicros(usage, pricing) * (1.0 + pricing.markupPercent / 100.0);
    const double rounded = std::ceil(raw - 1e-6);
    return rounded > 0.0 ? static_cast<std::int64_t>(rounded) : 0;
}

std::string formatUsd(std::int64_t micros) {
    const bool negative = micros < 0;
    const std::int64_t abs = negative ? -micros : micros;
    // Four decimals: round to the nearest hundred micro-dollars.
    const std::int64_t tenThousandths = (abs + 50) / 100;
    std::string frac = std::to_string(tenThousandths % 10'000);
    frac.insert(0, 4 - frac.size(), '0');
    return std::string(negative ? "-$" : "$") + std::to_string(tenThousandths / 10'000) + "." +
           frac;
}

Wallet::Wallet(WalletSettings settings)
    : settings_(settings), balance_(settings.starterCreditMicros) {}

bool Wallet::cloudAllowed() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return tier_ == AiTier::Paid && balance_ > 0;
}

AiTier Wallet::tier() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return tier_;
}

void Wallet::setTier(AiTier tier) {
    std::lock_guard<std::mutex> lock(mutex_);
    tier_ = tier;
}

std::int64_t Wallet::balanceMicros() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return balance_;
}

void Wallet::credit(std::int64_t micros) {
    if (micros <= 0) return;
    std::lock_guard<std::mutex> lock(mutex_);
    balance_ += micros;
}

std::int64_t Wallet::charge(const AnthropicUsage& oneResponse) {
    const std::int64_t price = priceMicros(oneResponse, settings_.pricing);
    const double cost = costMicros(oneResponse, settings_.pricing);
    std::lock_guard<std::mutex> lock(mutex_);
    // Clamped at both ends: never more than is left, and never negative, so a
    // bad price can only fail to charge, not mint money.
    const std::int64_t taken = std::clamp<std::int64_t>(price, 0, balance_);
    balance_ -= taken;
    sessionSpent_ += taken;
    sessionCost_ += cost;
    if (taken > 0) unsaved_ = true;
    if (balance_ == 0 && taken > 0 && tier_ == AiTier::Paid) ranOut_ = true;
    return taken;
}

bool Wallet::takeUnsavedCharges() {
    std::lock_guard<std::mutex> lock(mutex_);
    const bool pending = unsaved_;
    unsaved_ = false;
    return pending;
}

bool Wallet::takeRanOutEvent() {
    std::lock_guard<std::mutex> lock(mutex_);
    const bool fired = ranOut_;
    ranOut_ = false;
    return fired;
}

std::int64_t Wallet::sessionSpentMicros() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return sessionSpent_;
}

double Wallet::sessionCostMicros() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return sessionCost_;
}

bool Wallet::load(const std::filesystem::path& path) {
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) return true;  // new player: defaults stand
    const auto kv = readKv(path);
    std::lock_guard<std::mutex> lock(mutex_);
    if (auto it = kv.find("tier"); it != kv.end()) {
        tier_ = it->second == "paid" ? AiTier::Paid : AiTier::Free;
    }
    const auto it = kv.find("balance_micros");
    if (it == kv.end()) return false;
    // A save cut short mid-write can leave an empty or partial number; that is
    // reported (false) and the starter credit stands rather than crashing.
    const auto balance = parseMicros(it->second);
    if (!balance) return false;
    balance_ = std::max<std::int64_t>(0, *balance);
    return true;
}

bool Wallet::save(const std::filesystem::path& path) const {
    std::error_code ec;
    if (path.has_parent_path()) std::filesystem::create_directories(path.parent_path(), ec);
    AiTier tier;
    std::int64_t balance;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        tier = tier_;
        balance = balance_;
    }
    return setKvValue(path, "tier", tier == AiTier::Paid ? "paid" : "free") &&
           setKvValue(path, "balance_micros", std::to_string(balance));
}

}  // namespace llm_npc

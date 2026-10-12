#pragma once

#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string>

#include "AnthropicBackend.hpp"  // AnthropicUsage

namespace llm_npc {

// What the cloud model costs us and what a player pays for it. Prices are USD
// per million tokens, copied from the provider's pricing page into
// config/llm.cfg; the defaults are Claude Haiku 5.5 at prompts <= 100K tokens.
struct CloudPricing {
    double inputPerMTok = 0.10;
    double outputPerMTok = 0.50;
    double cacheWritePerMTok = 0.125;  // 5-minute cache write
    double cacheReadPerMTok = 0.01;
    // The margin: a player pays cost x (1 + markupPercent / 100).
    double markupPercent = 40.0;
};

// Wallet settings read from config/llm.cfg (price_*_per_mtok, markup_percent,
// starter_credit_usd). Missing keys keep the CloudPricing defaults.
struct WalletSettings {
    CloudPricing pricing;
    // Balance a brand-new wallet (no saves/wallet.cfg yet) starts with.
    std::int64_t starterCreditMicros = 1'000'000;
};

// Reads the wallet settings from configDir/llm.cfg. A missing, non-numeric or
// negative value keeps its default (and is logged), so the game always starts.
WalletSettings loadWalletSettings(const std::filesystem::path& configDir);

// Money is counted in integer micro-dollars (1 USD = 1'000'000) so that
// thousands of sub-cent charges add up exactly. The one place doubles appear is
// the per-response price below, which is rounded once into an integer.
constexpr std::int64_t kMicrosPerUsd = 1'000'000;

// Anthropic's exact cost of one response, in (fractional) micro-dollars.
// A token at P USD per million tokens costs exactly P micro-dollars.
double costMicros(const AnthropicUsage& usage, const CloudPricing& pricing);

// What the player is charged for one response: the cost with the markup
// applied, rounded UP to a whole micro-dollar so a charge never undercuts cost.
std::int64_t priceMicros(const AnthropicUsage& usage, const CloudPricing& pricing);

// "$4.9986": dollars with four decimals, enough to show a single reply's charge.
std::string formatUsd(std::int64_t micros);

// Which model a player's NPC conversations run on.
enum class AiTier { Free, Paid };

// The player's prepaid balance for the Paid tier. Purchases are simulated for
// now: credit() adds to the balance and no money moves. The worker thread
// charges it while the main thread reads it and buys, so every member locks.
class Wallet {
   public:
    // A new wallet: Free tier holding the starter credit.
    explicit Wallet(WalletSettings settings);

    // True when the cloud model may answer: Paid tier with money left.
    bool cloudAllowed() const;

    // The tier the player picked. Paid with a zero balance still reports Paid;
    // cloudAllowed() is what decides who answers.
    AiTier tier() const;
    // Switches tier; takes effect on the next request.
    void setTier(AiTier tier);

    // Money left, in micro-dollars; never negative.
    std::int64_t balanceMicros() const;
    // Adds a (simulated) purchase to the balance.
    void credit(std::int64_t micros);

    // Charges one response's usage and returns the amount taken. The balance
    // floors at zero: the response that crosses it was already generated, so
    // only what was left is taken. Reaching zero on Paid latches the ran-out
    // event once.
    std::int64_t charge(const AnthropicUsage& oneResponse);

    // True once after one or more charges since the last call, so the main
    // thread can persist spend without the worker thread doing file I/O.
    bool takeUnsavedCharges();

    // True exactly once after the balance runs out, for the HUD notice.
    bool takeRanOutEvent();

    // This session's takings and Anthropic's cost of them; the difference is
    // the margin. Not persisted.
    std::int64_t sessionSpentMicros() const;
    double sessionCostMicros() const;

    // The prices this wallet charges at; fixed after construction.
    const CloudPricing& pricing() const { return settings_.pricing; }

    // Reads `tier` and `balance_micros` from a key=value file. A missing file
    // is a new player: Free tier with the starter credit. Returns false if the
    // file existed but held no valid balance (the starter credit then stands).
    bool load(const std::filesystem::path& path);
    // Writes both keys, creating the file and its directory if needed.
    bool save(const std::filesystem::path& path) const;

   private:
    WalletSettings settings_;
    mutable std::mutex mutex_;
    AiTier tier_ = AiTier::Free;
    std::int64_t balance_ = 0;
    std::int64_t sessionSpent_ = 0;
    double sessionCost_ = 0.0;
    bool ranOut_ = false;
    bool unsaved_ = false;
};

}  // namespace llm_npc

// The Paid-tier wallet (plan: memoized-swinging-blossom): pricing one cloud
// response at cost plus markup, the prepaid balance it is charged against, and
// the saves/wallet.cfg round trip.
#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

#include "Wallet.hpp"
#include "doctest.h"

using namespace llm_npc;
namespace fs = std::filesystem;

namespace {

AnthropicUsage usageOf(std::uint64_t input, std::uint64_t output, std::uint64_t cacheWrite = 0,
                       std::uint64_t cacheRead = 0) {
    AnthropicUsage u;
    u.requests = 1;
    u.inputTokens = input;
    u.outputTokens = output;
    u.cacheWriteTokens = cacheWrite;
    u.cacheReadTokens = cacheRead;
    return u;
}

CloudPricing haikuAt(double markupPercent) {
    CloudPricing p;  // defaults are Haiku 5.5's per-million rates
    p.markupPercent = markupPercent;
    return p;
}

WalletSettings settingsWith(std::int64_t starterMicros, double markupPercent = 40.0) {
    WalletSettings s;
    s.pricing = haikuAt(markupPercent);
    s.starterCreditMicros = starterMicros;
    return s;
}

// A scratch directory removed when the test ends. Clock plus counter keeps
// names distinct across parallel runs, which an unseeded rand() would not.
struct TempDir {
    fs::path path;
    TempDir() {
        static std::atomic<int> counter{0};
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        path = fs::temp_directory_path() /
               ("llm_npc_wallet_" + std::to_string(stamp) + "_" + std::to_string(counter++));
        fs::create_directories(path);
    }
    ~TempDir() { fs::remove_all(path); }
};

}  // namespace

// --- Pricing -------------------------------------------------------------------

TEST_CASE("wallet: cost is tokens times the per-million rate, in micro-dollars") {
    // 1000 input at $0.10/M = 100 micros; 200 output at $0.50/M = 100 micros.
    CHECK(costMicros(usageOf(1000, 200), haikuAt(40)) == doctest::Approx(200.0));
}

TEST_CASE("wallet: priceMicros adds the markup to the cost") {
    CHECK(priceMicros(usageOf(1000, 200), haikuAt(40)) == 280);
}

TEST_CASE("wallet: priceMicros with no markup charges exactly the cost") {
    CHECK(priceMicros(usageOf(1000, 200), haikuAt(0)) == 200);
}

TEST_CASE("wallet: priceMicros rounds a fraction of a micro-dollar up, never down") {
    // 7 input tokens cost 0.7 micros; with 40% that is 0.98 -> 1.
    CHECK(priceMicros(usageOf(7, 0), haikuAt(40)) == 1);
    // One output token: 0.5 micros x 1.4 = 0.7 -> 1.
    CHECK(priceMicros(usageOf(0, 1), haikuAt(40)) == 1);
    // 2000 in + 300 out = 350 micros; x 1.25 = 437.5 -> 438.
    CHECK(priceMicros(usageOf(2000, 300), haikuAt(25)) == 438);
}

TEST_CASE("wallet: priceMicros does not let float noise bump an exact price") {
    // 100 micros x 1.1 is 110.00000000000001 in binary floating point; a bare
    // ceil would bill 111.
    CHECK(priceMicros(usageOf(1000, 0), haikuAt(10)) == 110);
}

TEST_CASE("wallet: priceMicros prices cache writes and reads at their own rates") {
    // 1000 cache-write at $0.125/M = 125; 1000 cache-read at $0.01/M = 10.
    CHECK(priceMicros(usageOf(0, 0, 1000, 1000), haikuAt(0)) == 135);
    CHECK(priceMicros(usageOf(0, 0, 1000, 1000), haikuAt(40)) == 189);
}

TEST_CASE("wallet: priceMicros of an empty usage block is zero") {
    CHECK(priceMicros(usageOf(0, 0), haikuAt(40)) == 0);
}

TEST_CASE("wallet: formatUsd shows four decimals") {
    CHECK(formatUsd(0) == "$0.0000");
    CHECK(formatUsd(5'000'000) == "$5.0000");
    CHECK(formatUsd(4'998'600) == "$4.9986");
    CHECK(formatUsd(280) == "$0.0003");
    CHECK(formatUsd(-1'500'000) == "-$1.5000");
}

// --- The balance ----------------------------------------------------------------

TEST_CASE("wallet: a new wallet is on Free with the starter credit") {
    Wallet w(settingsWith(1'000'000));
    CHECK(w.tier() == AiTier::Free);
    CHECK(w.balanceMicros() == 1'000'000);
}

TEST_CASE("wallet: the cloud is allowed only on Paid with money left") {
    Wallet w(settingsWith(1'000));
    CHECK_FALSE(w.cloudAllowed());  // Free
    w.setTier(AiTier::Paid);
    CHECK(w.cloudAllowed());

    Wallet empty(settingsWith(0));
    empty.setTier(AiTier::Paid);
    CHECK_FALSE(empty.cloudAllowed());
}

TEST_CASE("wallet: a charge takes the marked-up price and records the margin") {
    Wallet w(settingsWith(1'000'000));
    w.setTier(AiTier::Paid);
    CHECK(w.charge(usageOf(1000, 200)) == 280);
    CHECK(w.balanceMicros() == 1'000'000 - 280);
    CHECK(w.sessionSpentMicros() == 280);
    CHECK(w.sessionCostMicros() == doctest::Approx(200.0));
}

TEST_CASE("wallet: the balance floors at zero and the ran-out event fires once") {
    Wallet w(settingsWith(100));
    w.setTier(AiTier::Paid);
    CHECK_FALSE(w.takeRanOutEvent());

    // A 280-micro reply against a 100-micro balance takes only what is left.
    CHECK(w.charge(usageOf(1000, 200)) == 100);
    CHECK(w.balanceMicros() == 0);
    CHECK_FALSE(w.cloudAllowed());
    CHECK(w.takeRanOutEvent());
    CHECK_FALSE(w.takeRanOutEvent());

    // Further charges at zero take nothing and do not re-fire the event.
    CHECK(w.charge(usageOf(1000, 200)) == 0);
    CHECK_FALSE(w.takeRanOutEvent());
}

TEST_CASE("wallet: credit adds a purchase and ignores non-positive amounts") {
    Wallet w(settingsWith(0));
    w.credit(5 * kMicrosPerUsd);
    CHECK(w.balanceMicros() == 5'000'000);
    w.credit(0);
    w.credit(-10);
    CHECK(w.balanceMicros() == 5'000'000);
}

TEST_CASE("wallet: credits from two threads all land") {
    // The worker thread charges while the main thread buys and reads.
    Wallet w(settingsWith(0));
    std::vector<std::thread> threads;
    for (int t = 0; t < 2; ++t) {
        threads.emplace_back([&w] {
            for (int i = 0; i < 10'000; ++i) {
                w.credit(1);
                (void)w.balanceMicros();
            }
        });
    }
    for (std::thread& t : threads) t.join();
    CHECK(w.balanceMicros() == 20'000);
}

TEST_CASE("wallet: a charge marks the spend unsaved until it is taken") {
    Wallet w(settingsWith(1'000'000));
    w.setTier(AiTier::Paid);
    CHECK_FALSE(w.takeUnsavedCharges());
    w.charge(usageOf(1000, 200));
    CHECK(w.takeUnsavedCharges());
    CHECK_FALSE(w.takeUnsavedCharges());
    w.charge(usageOf(0, 0));  // nothing taken, nothing to save
    CHECK_FALSE(w.takeUnsavedCharges());
}

// --- Persistence ----------------------------------------------------------------

TEST_CASE("wallet: save then load keeps the tier and the balance") {
    TempDir dir;
    const fs::path file = dir.path / "saves" / "wallet.cfg";  // directory made on save
    {
        Wallet w(settingsWith(1'000'000));
        w.setTier(AiTier::Paid);
        w.charge(usageOf(1000, 200));
        REQUIRE(w.save(file));
    }
    Wallet loaded(settingsWith(1'000'000));
    CHECK(loaded.load(file));
    CHECK(loaded.tier() == AiTier::Paid);
    CHECK(loaded.balanceMicros() == 1'000'000 - 280);
}

TEST_CASE("wallet: loading a missing file keeps a new player's defaults") {
    TempDir dir;
    Wallet w(settingsWith(2'500'000));
    CHECK(w.load(dir.path / "absent.cfg"));
    CHECK(w.tier() == AiTier::Free);
    CHECK(w.balanceMicros() == 2'500'000);
}

TEST_CASE("wallet: settings read prices, markup and starter credit from llm.cfg") {
    TempDir dir;
    {
        std::ofstream out(dir.path / "llm.cfg");
        out << "price_input_per_mtok = 1.0\n"
            << "price_output_per_mtok = 5.0\n"
            << "price_cache_write_per_mtok = 1.25\n"
            << "price_cache_read_per_mtok = 0.1\n"
            << "markup_percent = 25\n"
            << "starter_credit_usd = 2.5\n";
    }
    const WalletSettings s = loadWalletSettings(dir.path);
    CHECK(s.pricing.inputPerMTok == doctest::Approx(1.0));
    CHECK(s.pricing.outputPerMTok == doctest::Approx(5.0));
    CHECK(s.pricing.cacheWritePerMTok == doctest::Approx(1.25));
    CHECK(s.pricing.cacheReadPerMTok == doctest::Approx(0.1));
    CHECK(s.pricing.markupPercent == doctest::Approx(25.0));
    CHECK(s.starterCreditMicros == 2'500'000);
}

TEST_CASE("wallet: settings without pricing keys keep the Haiku defaults") {
    TempDir dir;
    const WalletSettings s = loadWalletSettings(dir.path);
    CHECK(s.pricing.inputPerMTok == doctest::Approx(0.10));
    CHECK(s.pricing.outputPerMTok == doctest::Approx(0.50));
    CHECK(s.starterCreditMicros == 1'000'000);
}

TEST_CASE("wallet: a damaged balance keeps the starter credit instead of crashing") {
    // A save cut short mid-write, a hand edit, or a number too big for int64.
    for (const char* bad : {"", "abc", "1.5", "12abc", "99999999999999999999999"}) {
        CAPTURE(bad);
        TempDir dir;
        const fs::path file = dir.path / "wallet.cfg";
        {
            std::ofstream out(file);
            out << "tier = paid\nbalance_micros = " << bad << "\n";
        }
        Wallet w(settingsWith(2'500'000));
        CHECK_FALSE(w.load(file));
        CHECK(w.balanceMicros() == 2'500'000);
    }
}

TEST_CASE("wallet: a negative saved balance loads as zero") {
    TempDir dir;
    const fs::path file = dir.path / "wallet.cfg";
    {
        std::ofstream out(file);
        out << "balance_micros = -500\n";
    }
    Wallet w(settingsWith(2'500'000));
    CHECK(w.load(file));
    CHECK(w.balanceMicros() == 0);
}

TEST_CASE("wallet: bad pricing values keep their defaults") {
    TempDir dir;
    {
        std::ofstream out(dir.path / "llm.cfg");
        out << "price_input_per_mtok = abc\n"
            << "price_output_per_mtok = -1\n"
            << "price_cache_write_per_mtok = nan\n"
            << "price_cache_read_per_mtok = 1e999\n"
            << "markup_percent = -40\n"
            << "starter_credit_usd = -5\n";
    }
    const WalletSettings s = loadWalletSettings(dir.path);
    CHECK(s.pricing.inputPerMTok == doctest::Approx(0.10));
    CHECK(s.pricing.outputPerMTok == doctest::Approx(0.50));
    CHECK(s.pricing.cacheWritePerMTok == doctest::Approx(0.125));
    CHECK(s.pricing.cacheReadPerMTok == doctest::Approx(0.01));
    CHECK(s.pricing.markupPercent == doctest::Approx(40.0));
    CHECK(s.starterCreditMicros == 1'000'000);
}

TEST_CASE("wallet: an absurd starter credit is capped") {
    TempDir dir;
    {
        std::ofstream out(dir.path / "llm.cfg");
        out << "starter_credit_usd = 1e300\n";
    }
    CHECK(loadWalletSettings(dir.path).starterCreditMicros == 1'000'000 * kMicrosPerUsd);
}

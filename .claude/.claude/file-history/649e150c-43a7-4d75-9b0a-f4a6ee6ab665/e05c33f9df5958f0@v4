# Exercises 06–10: L4–L6 HFT Practice (C++ ULL + VHDL + Capstone)

## Context

The roadmap (`docs/learning/ROADMAP.md`) defines a 7-level curriculum. Exercises 01–05 are already built and all 51 tests are green. Levels 4–6 (C++ ultra-low-latency tier, VHDL extension, integration capstone) are marked "🔜 to build." This plan implements them using the identical exercises/solutions/tests/exercises pattern already established.

**Pattern recap (from exercise 04):**
- `exercises/0N_name/` — stub headers; unimplemented bodies throw `std::logic_error("not implemented")` → `main()` catches → exit 42 → pytest skips
- `solutions/0N_name/` — full implementations; same API
- `tests/exercises/cpp/test_0N_name.cpp` — ASSERT-macro test binary; exit 0/1/42
- `tests/exercises/test_0N_name.py` — pytest wrapper calling `run_cpp.sh`
- `tests/exercises/cpp/run_cpp.sh` — existing orchestrator, needs new exercises appended

**What already exists to reuse:**
- `solutions/01_order_book/order_book.py` — Python LOB semantics, FIFO fill logic, imbalance formula (C++ port of this is exercise 06)
- `solutions/04_spsc_ring/spsc_ring.hpp` — alignas(64), release/acquire atomics, exit-42 pattern (style reference for 06–08)
- `tests/exercises/cpp/test_spsc_ring.cpp` — ASSERT macro, try-catch exit-42 pattern (copy for 06–08 tests)
- `tests/exercises/cpp/run_cpp.sh` — just append exercise names to the tests array

---

## Exercise 06 — C++ Order Book (`06_cpp_order_book`)

**Goal:** Port the Python LOB (exercise 01) to C++ — same semantics, faster data structures. First place in this repo where data-structure choice directly maps to P&L throughput.

### Files to create
```
exercises/06_cpp_order_book/order_book.hpp    (stub)
solutions/06_cpp_order_book/order_book.hpp   (solution)
tests/exercises/cpp/test_06_order_book.cpp
tests/exercises/test_06_cpp_order_book.py
```

### Shared structs (top of both stub and solution)
```cpp
struct Order { int64_t order_id; int64_t price_ticks; int32_t qty; };
struct Fill  { int64_t order_id; int64_t price_ticks; int32_t qty; };
```
Price = integer ticks (×10000 of dollars) — no float equality bugs.

### `class OrderBook` — required interface (same in stub and solution)
```cpp
void add_limit(int64_t order_id, char side, int64_t price_ticks, int32_t qty);
void cancel(int64_t order_id);
std::vector<Fill> execute_market(char side, int32_t qty);
int64_t best_bid() const noexcept;   // INT64_MIN if empty
int64_t best_ask() const noexcept;   // INT64_MAX if empty
int32_t level_qty(char side, int64_t price_ticks) const noexcept;
double  imbalance(int levels = 1) const noexcept;
```

### Solution data structures
```cpp
std::map<int64_t, std::deque<Order>, std::greater<int64_t>> bids_; // largest-price first
std::map<int64_t, std::deque<Order>>                        asks_; // smallest-price first
std::unordered_map<int64_t, std::pair<char, int64_t>>       order_index_; // O(1) cancel
```

### Stub TODOs (6 clearly marked sections)
1. Declare `bids_`, `asks_`, `order_index_` data members (hint in comment)
2. Implement `add_limit` (route by side, deque.push_back, update index)
3. Implement `cancel` (index lookup, linear search in deque, erase empty level)
4. Implement `execute_market` (walk levels FIFO, partial-fill in place, return Fill list)
5. Implement `best_bid` / `best_ask` (one-liner each via rbegin/begin)
6. Implement `imbalance` (top-N level sum, (bid−ask)/(bid+ask))

### Tests (mirror Python test_01_order_book.py exactly)
- BBO sentinels on empty book
- Price priority (highest bid / lowest ask wins)
- FIFO within level: two same-price orders; first placed gets first fill
- Partial fill stays at queue front for next execution
- Market order walks multiple levels
- Cancel removes and re-derives BBO; cancel unknown ID → test FAIL (exit 1), not skip
- Imbalance: hand-computed value (same numbers as Python test: 30 bids / 10 asks → 0.5)
- Throughput smoke: 1,000,000 add+cancel cycles < 2 s wall-clock (catches O(n²) bugs)

### Exercise README — speed table
| Data structure | BBO | Insert | Cancel |
|---|---|---|---|
| `std::map` (this exercise) | O(log P) | O(log P) | O(log P + L) |
| Flat price array (L4 optimization) | O(1) amortized | O(1) | O(L) |

---

## Exercise 07 — Binary Feed Handler (`07_feed_handler`)

**Goal:** Parse a fixed-width binary message stream into book events with zero heap allocation.

### Files to create
```
exercises/07_feed_handler/feed_handler.hpp   (stub)
solutions/07_feed_handler/feed_handler.hpp   (solution)
tests/exercises/cpp/test_07_feed_handler.cpp
tests/exercises/test_07_feed_handler.py
```

### Message format (ITCH-inspired, packed)
```cpp
#pragma pack(push, 1)
struct MsgAdd    { char type; uint64_t oid; char side; int64_t price_ticks; int32_t qty; }; // 'A'
struct MsgCancel { char type; uint64_t oid; };                                               // 'X'
struct MsgExec   { char type; uint64_t oid; int32_t qty; };                                  // 'E'
struct MsgTrade  { char type; int64_t price_ticks; int32_t qty; };                           // 'P'
#pragma pack(pop)
```

### `class FeedHandler` — required interface
```cpp
struct BookEvent { char type; uint64_t oid; char side; int64_t price; int32_t qty; };

// parse one message; advance *pos by msg length; write to *out; return true if book-relevant
bool parse_one(const uint8_t* buf, size_t buf_len, size_t* pos, BookEvent* out);

// parse all; call callback(event) for each book event; return event count
template<typename Fn>
size_t parse_all(const uint8_t* buf, size_t buf_len, Fn callback);
```

### Stub TODOs
1. Implement the `switch(buf[*pos])` dispatch in `parse_one` for each message type
2. Use `std::memcpy` for field extraction (avoids undefined behavior from misaligned cast)
3. Implement `parse_all` loop advancing `pos` until `buf_len` exhausted

### Tests
- Round-trip: encode a known sequence (1 Add, 1 Cancel, 1 Exec, 1 Trade) → parse → verify events
- Buffer boundary: truncated message returns false without advancing pos
- Throughput: parse 1,000,000 Add messages in < 0.5 s

---

## Exercise 08 — Latency Benchmark Harness (`08_latency_bench`)

**Goal:** Measure and report p50/p99/p99.9 latency of exercises 06+07. The deliverable (a results table) is the portfolio artifact for low-latency interviews.

### Files to create
```
exercises/08_latency_bench/latency.hpp    (rdtsc + histogram — stub)
exercises/08_latency_bench/bench.cpp      (driver — partially pre-filled; TODOs for students)
solutions/08_latency_bench/latency.hpp
solutions/08_latency_bench/bench.cpp
tests/exercises/cpp/test_08_latency.cpp
tests/exercises/test_08_latency.py
```

### `latency.hpp` — required interface
```cpp
inline uint64_t rdtsc() noexcept;   // __rdtsc() intrinsic; fallback: clock_gettime MONOTONIC_RAW

class Histogram {
    static constexpr int BUCKETS = 64;   // bucket i: [2^i, 2^(i+1)) cycles
    uint64_t counts_[BUCKETS]{};
public:
    void record(uint64_t cycles) noexcept;           // student implements
    uint64_t percentile(double p) const noexcept;    // student implements
    void print(double mhz = 3000.0) const;           // pre-filled (converts cycles → ns)
};
```

### `bench.cpp` — pre-filled: setup, warm-up, MHz calibration, `hist.print()` call
**Student TODOs:** insert `rdtsc()` start/stop around `add_limit` and `execute_market`, call `hist.record(stop - start)`

### Tests
- `Histogram::percentile(0.5)` on 1024 known values returns within 1 bucket of expected
- `rdtsc()` monotonically increases over 1000 calls
- `bench.cpp` compiled binary produces stdout containing "p50" and "p99"

---

## Exercise 09 — Inventory-Aware VHDL Market Maker (`09_vhdl_inv_mm`)

**Goal:** Port exercise 02's `reservation_price` formula into VHDL (Q16.16 fixed-point), replacing the inventory-blind `fpga/market_maker.vhd`.

### Files to create
```
exercises/09_vhdl_inv_mm/inv_market_maker.vhd    (stub — skew formula left 0)
solutions/09_vhdl_inv_mm/inv_market_maker.vhd
tests/exercises/test_09_vhdl.py                  (ghdl driver; auto-skips if ghdl absent)
```

### VHDL entity (ports match existing style in fpga/)
```vhdl
entity inv_market_maker is
  port(
    clk        : in  std_logic;
    mid_price  : in  signed(31 downto 0);  -- Q16.16
    inventory  : in  signed(15 downto 0);
    max_inv    : in  signed(15 downto 0);
    gamma      : in  signed(31 downto 0);  -- ×1000
    sigma      : in  signed(31 downto 0);  -- ×1000
    T          : in  signed(31 downto 0);  -- ×1000
    bid_price  : out signed(31 downto 0);
    ask_price  : out signed(31 downto 0)
  );
end inv_market_maker;
```

Stub outputs bid_price = mid_price, ask_price = mid_price (zero skew). Student implements `r = mid - q*γ*σ²*T` and spread `s = γ*σ²*T + (2/γ)*ln(1+γ/k)` as fixed-point arithmetic.

---

## Exercise 10 — Capstone: Tick-to-Trade Integration (`10_tick_to_trade`)

**Goal:** Wire 06+07+strategy into one binary, replay a test feed, produce a latency table.

No stubs — a guided scaffold `main.cpp` with 5 numbered TODO comments and a Makefile with `-O0`/`-O2` targets.

Deliverable: print a p50/p99/p99.99 table. The student adds their own results table to the exercise README — the interview artifact.

### Files to create
```
exercises/10_tick_to_trade/main.cpp      (scaffold with 5 TODOs)
exercises/10_tick_to_trade/Makefile      (targets: bench_debug, bench_release)
solutions/10_tick_to_trade/main.cpp
data/sample_feed.bin                     (generated by scripts/gen_sample_feed.py)
scripts/gen_sample_feed.py               (Python helper, ~30 lines)
tests/exercises/test_10_tick_to_trade.py
```

---

## Build order

1. **06** (order book) — standalone, no dependencies, highest value
2. **07** (feed handler) — can test against 06
3. **08** (benchmark harness) — depends on 06+07 being testable
4. **09** (VHDL, independent track)
5. **10** (capstone, depends on all above)

Implement 06–08 in one session; 09 independently; 10 in a final session.

---

## Edits to existing files

| File | Change |
|---|---|
| `tests/exercises/cpp/run_cpp.sh` | Append `06_order_book`, `07_feed_handler`, `08_latency` to the `TESTS` array |
| `exercises/README.md` | Add rows 06–10 to exercise table |
| `docs/learning/ROADMAP.md` | Update L4–L6 status as each batch lands |
| `docs/learning/PROGRESS.md` | Add 06–10 to lessons↔exercises map |

---

## Verification

```bash
# After building 06–08:
HFT_EXERCISE_TARGET=solutions uv run pytest tests/exercises/ -v
# Expect: 51 + new tests pass; 3 VHDL skips remain

uv run pytest tests/exercises/ -v
# Expect: all new C++ exercises skipped (exit 42); Python exercises still skip

# After building 09:
bash scripts/run_vhdl_tests.sh
# Expect: PASS if ghdl installed, SKIP otherwise
```

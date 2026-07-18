---
name: project-exercise-curriculum
description: "State of the HFT practice exercise curriculum — which levels are built, what exists, what's next"
metadata: 
  node_type: memory
  type: project
  originSessionId: 649e150c-43a7-4d75-9b0a-f4a6ee6ab665
---

Exercises 01–08 are fully built and green (57 pytest skips on stubs, 54 pass on solutions + 3 VHDL skips).

**Built levels:**
- L0: Working system (paper_trading_sim.py, walkforward_sim.py, predict_next.py, analyze_equity.py)
- L1: Exercise 03 — stat-arb math (OLS hedge ratio, CUSUM, OTR, walk-forward splits)
- L2: Exercises 01–02 — Python order book + inventory-aware market maker (Avellaneda-Stoikov-lite)
- L3: Exercise 04 — C++20 O(1) ring MA + lock-free SPSC ring (alignas(64), release/acquire)
- L4: Exercises 06–08 — C++ order book (std::map + tick prices), binary feed handler (ITCH-inspired packed protocol), latency histogram (rdtsc + log buckets, p50/p99/p99.9)
- L5: Exercise 05 — VHDL testbenches (ghdl; auto-skipped without ghdl)

**Proposed (not yet built):**
- Exercise 09: inventory-aware VHDL market maker (port exercise 02's reservation_price into Q16.16 VHDL)
- Exercise 10: capstone tick-to-trade (feed handler → order book → strategy → latency table)

**Exercise conventions:**
- stubs: throw std::logic_error("not implemented") → exit 42 → pytest skip
- stub methods must NOT be noexcept if they throw (terminate() vs skip)
- run_cpp.sh maps test name → include dirs via case statement
- HFT_EXERCISE_TARGET=solutions switches all includes to solutions/

**Why:** User wants to learn algo trading from daily-bar ML through HFT to get a high-paying role. Curriculum follows the latency ladder: Python (L0-L2) → low-latency C++ (L3-L4) → FPGA (L5) → capstone (L6).

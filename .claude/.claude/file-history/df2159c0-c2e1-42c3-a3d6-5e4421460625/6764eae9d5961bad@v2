---
name: project-shorting-distinction
description: "Two distinct \"shorting\" concepts in this project — do not conflate them"
metadata: 
  node_type: memory
  type: project
  originSessionId: df2159c0-c2e1-42c3-a3d6-5e4421460625
---

There are two separate concepts both called "shorting" in this codebase:

1. **Intraday short-side trade** — taking the bearish signal from a candlestick pattern (e.g., bearish engulfing), opening a short position intraday, and closing it same session. This is part of the `strategies/` day-trading engine. Paper-short is enabled by default with a `--no_short` flag to restrict to long-only.

2. **Stock shorting as an investment strategy** — issue #6 ("Create shorting option to the code"). This applies to the position-level portfolio in `walkforward_sim.py` / `predict_next.py`, allowing the GBM-driven portfolio to hold short positions as a risk-limiting tool. This is a separate feature, not yet planned in detail.

**Why:** User explicitly flagged this distinction to avoid confusion when both topics come up in the same conversation.

**How to apply:** When discussing "shorting" in a day-trading or candlestick context, clarify it means intraday short trades. When discussing issue #6 or the GBM portfolio, clarify it means portfolio-level short positions. Never conflate them.

---
description: Overnight development session for agentic trading repos. Works through ready-for-ai issues with strict risk controls — execution layer is OFF LIMITS for autonomous changes. Generates learning materials per feature.
---

You are running an overnight autonomous session on the trading repository.

Follow the universal overnight-session-base workflow PLUS the
trading-specific overrides below.

Read and follow overnight-session-base.md first, then apply these:

⚠️ READ THIS FIRST — RISK POLICY ⚠️

Trading code has unique failure modes. A bug in the wrong place can cause
real financial loss in seconds. This overnight session has a hard boundary
between code that is safe to change autonomously and code that is NOT.

---

## TRADING-SPECIFIC: WHAT IS IN SCOPE TONIGHT

SAFE for autonomous overnight work:
- Backtesting logic and strategy research code
- Data ingestion and processing pipelines
- Analysis, visualization, and reporting
- Test coverage additions
- Documentation
- Performance improvements to non-execution code

OUT OF SCOPE — do NOT touch autonomously, ever:
- Order management system (OMS)
- Order submission, cancellation, or modification logic
- Position sizing calculations for live trading
- Kill switches and circuit breakers
- Live execution infrastructure
- Broker API integration code
- Anything in live/ or execution/ directories

If an issue touches out-of-scope code, label it needs-human immediately:
```bash
gh issue edit [N] --add-label "needs-human" --remove-label "ready-for-ai"
gh issue comment [N] --body "This issue touches execution-layer code which is out of scope for autonomous overnight work. Requires human review and manual implementation."
```

Do not attempt it. Do not try to implement "just the safe parts."

---

## TRADING-SPECIFIC: SPECIALIST AGENTS

| Issue type | Primary agent | Secondary agent |
|------------|--------------|-----------------|
| Backtesting logic | backtesting-verifier (review after) | — |
| Strategy research | backtesting-verifier (review after) | — |
| Data pipeline | market-data-validator (review after) | — |
| Analysis/visualization | (main agent) | — |
| Test coverage | tester | test-auditor |
| Documentation | (main agent) | — |
| Performance (non-execution) | (main agent) | — |

For ANY change to backtesting logic or strategy code:
Always run backtesting-verifier after implementing, before committing.
It will check for lookahead bias, leakage, and execution realism.

For ANY change to data ingestion:
Always run market-data-validator after implementing, before committing.
It will check timezone handling, corporate actions, and stale data.

---

## TRADING-SPECIFIC: TEST COMMANDS

```bash
# Python tests
pytest tests/ -v

# Specifically for backtest changes — include integrity checks
pytest tests/backtesting/ -v
pytest tests/data/ -v

# For data pipeline changes
python3 tests/validate_data_integrity.py

# No live trading tests — paper trading environment only
```

If any test suggests it touches live execution, stop and label needs-human.

---

## TRADING-SPECIFIC: HARD RULES

1. Never touch execution layer code (OMS, order submission, position sizing)
2. If in doubt about whether something is execution-layer, it is. Skip it.
3. Backtesting changes must pass the backtesting-verifier check before commit
4. Data pipeline changes must pass the market-data-validator check before commit
5. No new dependencies added to the live trading environment without human review
6. All test additions must use historical/paper data only — never live feeds
7. If you find a bug in the execution layer while working on something else,
   document it in a new GitHub issue but do NOT fix it autonomously

---

## TRADING-SPECIFIC: LEARNING CONCEPT SELECTION

For backtesting work:
- Concept = the statistical or algorithmic concept
- "Lookahead bias in financial backtesting" ✅
- "Updated the backtest runner" ❌

For data pipeline:
- Concept = the data quality or financial data concept
- "Point-in-time market data and survivorship bias" ✅
- "Fixed the data ingestion" ❌

For strategy research:
- Concept = the quantitative finance or signal concept
- "Mean reversion vs momentum: when each applies" ✅
- "Added a new signal" ❌

For analysis/visualization:
- Concept = the analytical technique
- "Sharpe ratio and risk-adjusted return measurement" ✅
- "Made a new chart" ❌

---

## TRADING-SPECIFIC: OVERNIGHT LAUNCHER

Save as ~/scripts/nightly-trading.sh:

```bash
#!/bin/bash
set -euo pipefail

REPO="$HOME/repos/trading"   # adjust to your actual path
LOG="$REPO/nightly-$(date +%Y%m%d).log"

echo "$(date): Starting trading overnight session" | tee "$LOG"
echo "SCOPE: Backtesting, data, analysis ONLY. Execution layer is OFF LIMITS." | tee -a "$LOG"

cd "$REPO"
git checkout main
git pull

claude --dangerously-skip-permissions \
  "Run /overnight-session for the trading repository. Tonight's issues are labeled ready-for-ai. IMPORTANT: The execution layer (OMS, order submission, position sizing, kill switches) is completely out of scope — label any such issues needs-human immediately. Only work on backtesting, data pipelines, analysis, and test coverage. Use backtesting-verifier after any strategy change and market-data-validator after any data pipeline change. Generate learning materials for each completed feature and rebuild the index at the end." \
  2>&1 | tee -a "$LOG"

echo "$(date): Session complete. Check OVERNIGHT_REPORT.md and draft PRs." | tee -a "$LOG"
```

---
name: HFT Practice Project Overview
description: Core facts about the HFT_practice codebase — architecture, stack, key files, working branch
type: project
originSessionId: b67514e4-0682-4237-9169-ed2a74e3ea2e
---
Python + C++ high-frequency/algorithmic trading system. Code was originally written by Codex.

**Stack**: Python 3.12, uv, scikit-learn GradientBoostingRegressor, yfinance, FastAPI, Tkinter, ONNX Runtime (C++), robin-stocks (Robinhood)

**Key workflows**: paper backtest → walk-forward sim → predict next day → Robinhood execution

**Working branch**: `Claude_Fix_Ups_and_Documentations` (remote: origin/Claude_Fix_Ups_and_Documentations)

**Key entry points**:
- `paper-backtest` → paper_trading_sim.py
- `walkforward-sim` → walkforward_sim.py
- `predict-next` → predict_next.py
- `gui-hft` → gui_app.py (Tkinter)
- `web-hft` → webapp/main.py (FastAPI)
- `universe-builder` → scripts/universe_builder.py
- `rh-exec` → scripts/robinhood_executor.py (Robinhood, default dry-run)

**Data**: ~500 OHLCV CSVs in data/, model bundles in models/, ticker lists in tickers/

**Best model so far**: model_bundle_11_14_2025_sap_500_2016_to_2020.joblib — ~27.19% annualized return (tested 2020–2025)

**C++ files**: sample_algorithm.cpp (market maker), sample_with_linear_regression.cpp, gradient_boosting/ (ONNX inference, GB sim)

**Why:** User wants to expand to iOS app, FPGA/VHDL, Docker deployment, full docs, unit tests, web hosting files, dynamic ticker fetching.
**How to apply:** Always commit to Claude_Fix_Ups_and_Documentations branch. python-docx is installed. uv is used for dependency management.

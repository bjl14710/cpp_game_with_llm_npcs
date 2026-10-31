# Plan: Crude-Oil Lead-Lag Research Pipeline + Streamlit Dashboard

## Context

The repo (`HFT_Trading_Practice_Repo`) is currently empty except for a one-line `README.md`.
The goal is to bootstrap a quant-research starter kit grounded in `crude_oil_research.txt`:
a daily **CL-vs-XLE lead-lag study** with a **Streamlit GUI** that shows pipeline
**progress** and **projected costs**.

This is a learning/research tool for a solo quant, not production trading. The paper's
key traps must be respected: the April 20 2020 negative WTI print (−$37.63) breaks log
returns, `CL=F` is a rolled continuous series with roll-day discontinuities, and free-tier
API limits are tight and must be tracked.

**Decisions locked in with the user:**
- GUI: **Streamlit** (pure-Python dashboard)
- "Projected costs" = **all three**: (a) API usage vs free-tier budget, (b) hypothetical
  trading transaction costs, (c) compute/time cost per stage
- First milestone scope: **full CL-vs-XLE daily pipeline (paper Stages 1–3)** —
  download → clean → lead-lag battery → dashboard. Microstructure (Kyle/OFI/Avellaneda-Stoikov)
  is explicitly deferred (needs order-book data the free sources don't provide).

## Outcome

Run `streamlit run app/dashboard.py`, click "Run pipeline", and watch each stage
(download, clean, analyze) progress live, with a results panel (CCF plot, Granger p-values,
cointegration verdict) and a costs panel (API requests used/remaining, est. transaction
cost in bp, per-stage runtime). Everything also runnable headless via `python -m oil_alpha.pipeline`.

## Proposed structure

```
HFT_Trading_Practice_Repo/
├── README.md                      # update: setup + run instructions
├── requirements.txt               # pinned deps
├── .env.example                   # FRED_API_KEY, EIA_API_KEY, etc. (optional keys)
├── pyproject.toml                 # package metadata, ruff/pytest config
├── src/oil_alpha/
│   ├── __init__.py
│   ├── config.py                  # symbols, date range, free-tier caps, cost params
│   ├── data/
│   │   ├── sources.py             # fetch_yf(), fetch_fred(), fetch_stooq()
│   │   ├── budget.py              # APIBudget: track requests vs daily caps
│   │   └── cache.py               # parquet cache in ./data_cache/
│   ├── clean.py                   # returns(), handle April-2020, flag roll days
│   ├── analysis/
│   │   ├── leadlag.py             # ccf_leadlag(), granger_both_directions()
│   │   ├── cointegration.py       # engle_granger(), johansen()
│   │   └── costs.py               # transaction_cost_bp()
│   ├── metrics.py                 # StageTimer / compute-time tracking
│   └── pipeline.py                # orchestrates stages, yields progress events
├── app/
│   └── dashboard.py               # Streamlit UI
└── tests/
    ├── test_clean.py              # 2020 negative-price + roll handling
    ├── test_leadlag.py           # CCF sign convention on synthetic lead
    └── test_budget.py             # cap accounting
```

## Implementation steps

### 1. Project scaffold & deps
- `requirements.txt`: `yfinance pandas numpy statsmodels scipy fredapi pandas-datareader
  streamlit plotly pyarrow python-dotenv pytest ruff`.
- `pyproject.toml` with package `oil_alpha` (src layout), ruff + pytest config.
- `.env.example` with `FRED_API_KEY=`, `EIA_API_KEY=` (both optional — FRED CSV and
  yfinance need no key).

### 2. `config.py`
- `SYMBOLS = {"wti_fut": "CL=F", "energy_etf": "XLE", "oil_etf": "USO", "xom": "XOM", "cvx": "CVX"}`.
- `FRED_SERIES = {"wti_spot": "DCOILWTICO"}`.
- Default date range (e.g. 2015→today).
- `FREE_TIER_CAPS = {"alpha_vantage": 25, "twelve_data": 800, "fmp": 250, ...}` (per paper Part 1).
- Transaction-cost params: assumed half-spread bp + commission bp per asset.

### 3. Data layer (`data/sources.py`, `budget.py`, `cache.py`)
- `fetch_yf(symbols, start, end)` — **always pass `auto_adjust=True` explicitly** (paper notes
  the 2025 default flip). Guard against the `WTI`-is-W&T-Offshore gotcha by only using config symbols.
- `fetch_fred(series)` — `DCOILWTICO` daily WTI spot; CSV path so no key required; use `fredapi` if key present.
- `fetch_stooq(symbols)` — equities fallback with `.US` suffix; **do not** use it for crude futures
  (broken since 2021 per paper).
- `APIBudget` in `budget.py` — increments per request, exposes `used`/`remaining` against
  `FREE_TIER_CAPS`, raises/warns before exceeding. yfinance/FRED counted as "unlimited" but still tallied for the compute panel.
- `cache.py` — parquet cache keyed by (symbol, range) so re-runs don't re-hit APIs.

### 4. Cleaning (`clean.py`) — the paper's traps
- `to_returns(prices, kind="log")` — **assert all prices > 0 before log**; raise a clear error
  pointing at the April-2020 window otherwise.
- `handle_negative_window(df)` — for `CL=F` across 2020-04-20, switch to simple returns / dollar
  changes for that window (or substitute USO/`DCOILWTICO` spot proxy), and flag it.
- `flag_roll_days(cl_series)` — detect front-month roll discontinuities; option to drop roll-day returns.
- Output: aligned, stationary return frame with a `flags` column, no inf/NaN.

### 5. Analysis (`analysis/leadlag.py`, `cointegration.py`)
- `ccf_leadlag(x, y, max_lag)` — `statsmodels.tsa.stattools.ccf`; **validate sign convention
  against a synthetic known lead** (test); return lag of peak + the curve for plotting.
- `granger_both_directions(x, y, maxlag)` — run CL→XLE and XLE→CL, pick lag by AIC, return p-values both ways.
- `engle_granger(p_xle, p_cl)` — `coint()` on price *levels*; `johansen(prices)` —
  `coint_johansen` for the multi-series version. Return a plain-English verdict.
- `costs.py::transaction_cost_bp()` — given a hypothetical signal turnover, estimate round-trip
  bp cost from config half-spread + commission. Standalone, dashboard-displayed.

### 6. Orchestration (`pipeline.py`, `metrics.py`)
- `run_pipeline(...)` as a **generator that yields progress events**
  `{stage, status, pct, payload}` so both Streamlit and the CLI can consume the same stream.
- `metrics.py::StageTimer` context manager records wall-clock + row counts per stage for the compute-cost panel.
- `if __name__ == "__main__"` → headless run printing each event + a final summary.

### 7. Streamlit dashboard (`app/dashboard.py`)
- **Sidebar**: symbol/date pickers, return type, max lag, "Run pipeline" button, optional API keys.
- **Progress panel**: `st.status`/progress bar driven by the pipeline generator, one row per stage.
- **Results panel**: Plotly CCF curve (lag of peak annotated), Granger p-value table (both directions),
  cointegration verdict, rolling-correlation + XLE-beta-to-oil mini charts.
- **Costs panel** (three sub-sections matching the user's pick):
  (a) API budget — used/remaining bars per source vs `FREE_TIER_CAPS`;
  (b) Transaction cost — est. round-trip bp for the hypothetical lead-lag trade;
  (c) Compute — per-stage runtime + rows processed from `StageTimer`.
- Cache results in `st.session_state` so re-renders don't re-run the pipeline.

### 8. Tests (`tests/`)
- `test_clean.py` — log-returns raises on the negative-price window; roll-day flagging works.
- `test_leadlag.py` — CCF correctly identifies a synthetically injected lead (locks the sign convention).
- `test_budget.py` — budget accounting and cap-exceeded warning.

## Reuse / grounding notes
- All methods map directly to `crude_oil_research.txt` Parts 1–2 and the Stage 1–3 recommendations.
- Lean on `statsmodels` built-ins (`ccf`, `grangercausalitytests`, `coint`, `coint_johansen`) rather
  than hand-rolling — the paper names these exact functions.
- No existing code to reuse (greenfield repo); README is the only current file and will be updated.

## Verification
1. `pip install -r requirements.txt`.
2. `pytest` — all three test modules pass (esp. the 2020 negative-price guard and CCF sign convention).
3. Headless: `python -m oil_alpha.pipeline` — prints stage progress and a summary with a non-empty
   CCF peak lag and Granger p-values; expect **CL leads XLE at short lags** under the default range
   (paper's predicted result; opposite ⇒ suspect a sign/alignment bug).
4. GUI: `streamlit run app/dashboard.py` → click "Run pipeline" → confirm progress advances through
   all stages and the Results + three Costs sub-panels populate.
5. Sanity cross-check (paper Stage-1 threshold): `CL=F` close vs FRED `DCOILWTICO` agree to within a
   few % on common dates (different instruments — spot vs rolled future — so not bp-exact).

## Out of scope (future milestones)
- Microstructure alpha (Kyle's lambda, OFI, microprice, Avellaneda-Stoikov) — needs order-book/tick data.
- Intraday/Hayashi-Yoshida lead-lag — needs paid tick data.
- Live trading / order routing.

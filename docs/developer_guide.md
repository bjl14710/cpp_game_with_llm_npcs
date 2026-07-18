# Silmulator Developer Guide

**Version 1.0.0**

---

## Table of Contents

1. [Repository Layout](#repository-layout)
2. [Architecture Overview](#architecture-overview)
3. [SCPI Parser](#scpi-parser)
   - [Python Parser](#python-parser)
   - [C++ Parser (optional)](#c-parser-optional)
4. [Device Layer](#device-layer)
   - [BaseDevice](#basedevice)
   - [Handler Registration Pattern](#handler-registration-pattern)
   - [State Management](#state-management)
   - [Error Queue](#error-queue)
5. [GUI Layer](#gui-layer)
   - [MainWindow](#mainwindow)
   - [CanvasScene and CanvasView](#canvasscene-and-canvasview)
   - [BaseBlock](#baseblock)
   - [ConnectionPort and Wires](#connectionport-and-wires)
   - [BLOCK_REGISTRY](#block_registry)
6. [Signal Bus](#signal-bus)
7. [Code Parser](#code-parser)
8. [Waveform Utilities](#waveform-utilities)
9. [Testing](#testing)
10. [Build System](#build-system)
11. [Adding a New Device (Step-by-Step)](#adding-a-new-device-step-by-step)
12. [Adding a New GUI Block (Step-by-Step)](#adding-a-new-gui-block-step-by-step)
13. [Coding Conventions](#coding-conventions)

---

## Repository Layout

```
Silmulator/
│
├── main.py                    # CLI entry: calls silmulator.app.main()
├── setup.py                   # setuptools entry for pip install -e .
├── pyproject.toml             # PEP 517 package metadata
├── requirements.txt           # Runtime Python dependencies
├── CMakeLists.txt             # Root C++ build (finds pybind11, recurses into cpp_core/)
│
├── cpp_core/                  # Optional high-performance C++ SCPI parser
│   ├── CMakeLists.txt
│   ├── include/
│   │   ├── scpi_parser.h      # ScpiParser class declaration
│   │   └── scpi_types.h       # ScpiCommand, ScpiToken, ScpiResponse types
│   └── src/
│       ├── scpi_parser.cpp    # Parser implementation (C++17)
│       └── bindings.cpp       # pybind11 Python bindings
│
├── silmulator/                # Main Python package
│   ├── __init__.py            # Package-level exports, version string
│   ├── app.py                 # Application entry point: main() starts Qt loop
│   ├── main.py                # Thin shim for `python -m silmulator`
│   │
│   ├── devices/               # SCPI instrument models
│   │   ├── __init__.py        # Re-exports all device classes
│   │   ├── base_device.py     # BaseDevice, ScpiError, ERROR_MSGS
│   │   ├── oscilloscope.py    # Oscilloscope (Tektronix MSO54-compatible)
│   │   ├── power_supply.py    # PowerSupply (Keysight E36312A-compatible)
│   │   ├── function_generator.py  # FunctionGenerator (Keysight 33522B-compatible)
│   │   ├── multimeter.py      # Multimeter (Keysight 34401A-compatible)
│   │   └── msp430.py          # MSP430 (TI MSP430FR5994-compatible)
│   │
│   ├── scpi/                  # SCPI parsing
│   │   ├── __init__.py
│   │   └── parser.py          # ScpiParser, ScpiCommand (pure Python)
│   │
│   ├── code_parser/           # Source code analysis
│   │   ├── __init__.py
│   │   └── base_parser.py     # CodeParser: extracts SCPI from C/C++/Python
│   │
│   ├── gui/                   # PyQt6 application
│   │   ├── __init__.py
│   │   ├── main_window.py     # MainWindow, BlockLibraryPanel, ConsolePanel
│   │   ├── canvas.py          # CanvasScene, CanvasView, BLOCK_REGISTRY
│   │   └── blocks/
│   │       ├── __init__.py
│   │       ├── base_block.py  # BaseBlock (abstract), ConnectionPort, WireItem
│   │       ├── oscilloscope_block.py
│   │       ├── power_supply_block.py
│   │       ├── function_generator_block.py
│   │       ├── multimeter_block.py
│   │       ├── msp430_block.py
│   │       └── code_input_block.py
│   │
│   └── utils/
│       ├── __init__.py
│       ├── waveform.py        # WaveformType enum, generate_waveform()
│       └── signals.py         # SignalBus singleton (Qt signals)
│
├── tests/                     # pytest suite
│   ├── test_oscilloscope.py
│   ├── test_power_supply.py
│   ├── test_function_generator.py
│   ├── test_multimeter.py
│   ├── test_msp430.py
│   └── test_scpi_parser.py
│
├── docs/                      # Documentation
│   ├── user_guide.md
│   ├── developer_guide.md     # ← you are here
│   ├── adding_blocks.md
│   ├── architecture.md
│   └── Doxyfile
│
├── docker/                    # Docker support files
│   ├── entrypoint.sh
│   └── README.md
│
├── Dockerfile                 # Production multi-stage build
├── Dockerfile.dev             # Development image (X11 forwarding)
├── docker-compose.yml         # Production (NoVNC web browser)
└── docker-compose.dev.yml     # Development (native display)
```

---

## Architecture Overview

Silmulator has three horizontal layers:

```
┌──────────────────────────────────────────────────────┐
│                   GUI Layer (PyQt6)                  │
│   MainWindow → CanvasScene → BaseBlock subclasses    │
│           ↕  SignalBus (Qt signals)  ↕               │
├──────────────────────────────────────────────────────┤
│                  Device Layer (Python)                │
│   BaseDevice → Oscilloscope / PowerSupply / etc.     │
│           ↕  SCPI dispatch  ↕                        │
├──────────────────────────────────────────────────────┤
│               SCPI Parser Layer (Python + C++)        │
│   ScpiParser → [ScpiCommand, ...]                    │
└──────────────────────────────────────────────────────┘
```

**Data flow for a SCPI command:**

```
User types: "CH1:SCALE 0.5"
     │
     ▼
GUI (ConsolePanel / Block right-click)
     │ calls block.device.process("CH1:SCALE 0.5")
     ▼
BaseDevice.process()
     │ ScpiParser.parse("CH1:SCALE 0.5")
     ▼
ScpiParser → [ScpiCommand(path="CH1:SCALE", is_query=False, params=["0.5"])]
     │
     ▼
BaseDevice._dispatch(cmd)
     │ lookup self._handlers["CH1:SCALE"]
     ▼
Oscilloscope._ch_scale(cmd)  →  self.channels[1].scale = 0.5
     │ return "OK" (or empty string for set commands)
     ▼
Response → Console display
```

---

## SCPI Parser

### Python Parser

**File:** `silmulator/scpi/parser.py`

The pure-Python parser handles the full SCPI command grammar:
- Compound commands separated by `;`
- Query suffix `?`
- Short-form aliasing (e.g., `VOLT` → `VOLTAGE`, `MEAS` → `MEASURE`)
- Quoted string parameters
- Relative path tracking (`:` prefix continues from last root)

**Key classes:**

```python
@dataclass
class ScpiCommand:
    path: str          # Normalized, uppercased header: "TRIGGER:A:EDGE:SOURCE"
    is_query: bool     # True if command ends with '?'
    params: list[str]  # Parameters as raw strings: ["0.5", "\"ON\""]

class ScpiParser:
    def parse(self, raw: str) -> list[ScpiCommand]: ...
    def _split_compound(self, raw: str) -> list[str]: ...
    def _parse_single(self, cmd: str) -> ScpiCommand: ...
    def _normalize(self, path: str) -> str: ...
```

**Parser selection in BaseDevice:**

```python
# base_device.py
try:
    from silmulator.scpi import scpi_core  # C++ extension
    self._parser = scpi_core.ScpiParser()
except ImportError:
    from silmulator.scpi.parser import ScpiParser
    self._parser = ScpiParser()  # Python fallback
```

### C++ Parser (optional)

**Files:** `cpp_core/include/scpi_parser.h`, `cpp_core/src/scpi_parser.cpp`

The C++ parser provides the same interface as the Python version but runs 10-20× faster for high-rate command streams (firmware replay, large test suites).

**C++ types:**

```cpp
namespace silmulator {

struct ScpiCommand {
    std::string path;           // "TRIGGER:A:EDGE:SOURCE"
    bool is_query;
    std::vector<std::string> params;
};

class ScpiParser {
public:
    std::vector<ScpiCommand> parse(const std::string& raw);
    static std::string normalize_header(const std::string& hdr);
    static std::vector<std::string> split_compound(const std::string& raw);
    static std::vector<ScpiToken> tokenize(const std::string& cmd);
private:
    std::string current_root_;  // tracks relative path state
};

} // namespace silmulator
```

**pybind11 bindings** (`bindings.cpp`):

```cpp
PYBIND11_MODULE(scpi_core, m) {
    py::class_<ScpiCommand>(m, "ScpiCommand")
        .def_readonly("path", &ScpiCommand::path)
        .def_readonly("is_query", &ScpiCommand::is_query)
        .def_readonly("params", &ScpiCommand::params);

    py::class_<ScpiParser>(m, "ScpiParser")
        .def(py::init<>())
        .def("parse", &ScpiParser::parse);
}
```

**Building:**

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
cp scpi_core*.so ../silmulator/scpi/
```

---

## Device Layer

### BaseDevice

**File:** `silmulator/devices/base_device.py`

All simulated instruments inherit from `BaseDevice`. It provides:

| Method | Purpose |
|---|---|
| `process(raw: str) -> str` | Parse and dispatch a raw SCPI string; return response |
| `reset()` | Call `_do_reset()` on the subclass |
| `_register(path, handler)` | Register one handler for a command path |
| `_register_aliases(paths, handler)` | Register handler for multiple paths at once |
| `_push_error(code, msg)` | Add to the error queue (max 20 entries) |
| `_pop_error() -> tuple` | Return and remove oldest error |

**Subclass contract:**

```python
class MyDevice(BaseDevice):
    def __init__(self):
        super().__init__()
        self._setup_handlers()   # must register all handlers here

    def _setup_handlers(self):
        self._register("MY:CMD", self._my_cmd)
        self._register_aliases(["MY:COMMAND", "MY:CMD"], self._my_cmd)

    def _do_reset(self):
        # return all state to power-on defaults
        self.my_value = 0.0

    def _my_cmd(self, cmd: ScpiCommand) -> str:
        if cmd.is_query:
            return str(self.my_value)
        self.my_value = float(cmd.params[0])
        return ""
```

### Handler Registration Pattern

Every handler is a `Callable[[ScpiCommand], str]`. The return value is:
- **Empty string `""`** for set commands (no response).
- **A value string** for query commands — the caller joins multiple responses with `";"`.

Both the long form and all recognized short forms of a command are registered:

```python
# oscilloscope.py example
self._register_aliases(
    ["TRIGGER:A:EDGE:SOURCE", "TRIG:A:EDGE:SOURCE",
     "TRIGGER:A:EDGE:SOU",   "TRIG:A:EDGE:SOU"],
    self._trig_edge_source
)
```

This means you can send `TRIG:A:EDGE:SOU CH1` or `TRIGGER:A:EDGE:SOURCE CH1` and both reach the same handler.

### State Management

Each device stores its state in Python dataclasses, one per logical grouping:

```python
@dataclass
class ChannelState:
    scale: float = 1.0
    offset: float = 0.0
    coupling: str = "DC"
    bandwidth: float = 500e6
    enabled: bool = True

# In Oscilloscope.__init__:
self.channels: dict[int, ChannelState] = {
    1: ChannelState(), 2: ChannelState(),
    3: ChannelState(), 4: ChannelState(),
}
```

All state is reset by `*RST` → `_do_reset()` which re-initializes these dictionaries.

### Error Queue

SCPI error queue follows the IEEE 488.2 standard:

```python
# Push an error
self._push_error(-113, "Undefined header")

# Retrieve via SCPI
device.process("SYST:ERR?")   # Returns "-113,\"Undefined header\""
device.process("*CLS")        # Clears all errors
```

---

## GUI Layer

### MainWindow

**File:** `silmulator/gui/main_window.py`

`MainWindow(QMainWindow)` creates the overall shell:
- **Menu bar**: File, Edit, View, Help
- **Left dock**: `BlockLibraryPanel` (QListWidget of block types)
- **Central widget**: `CanvasView`
- **Bottom dock**: `ConsolePanel` (QPlainTextEdit + QLineEdit)

### CanvasScene and CanvasView

**File:** `silmulator/gui/canvas.py`

`CanvasScene(QGraphicsScene)` owns:
- All `BaseBlock` items
- All `WireItem` items
- `BLOCK_REGISTRY`: dict mapping device-type strings to block classes

`CanvasView(QGraphicsView)` wraps the scene and handles:
- Zoom (Ctrl+scroll or scroll)
- Pan (middle-mouse drag)
- Rubber-band selection (left-drag on empty space)

**BLOCK_REGISTRY** (in `canvas.py`):

```python
BLOCK_REGISTRY: dict[str, type[BaseBlock]] = {
    "Oscilloscope":      OscilloscopeBlock,
    "Power Supply":      PowerSupplyBlock,
    "Function Generator": FunctionGeneratorBlock,
    "Multimeter":        MultimeterBlock,
    "MSP430":            MSP430Block,
    "Code Input":        CodeInputBlock,
}
```

### BaseBlock

**File:** `silmulator/gui/blocks/base_block.py`

`BaseBlock(QGraphicsRectItem)` is the abstract base for all canvas blocks.

Key responsibilities:
- Holds a reference to its `device` (`BaseDevice` subclass instance)
- Draws the block body, header label, and type badge
- Manages `ConnectionPort` child items on each edge
- Right-click context menu: "Send SCPI command", "Reset device", "Properties"
- Drag-move handling

Subclasses must implement:
```python
@property
def device_type(self) -> str: ...     # e.g., "Oscilloscope"
def _create_device(self) -> BaseDevice: ...  # instantiate the device
def _build_ports(self) -> None: ...   # call self._add_port(...) for each port
```

### ConnectionPort and Wires

`ConnectionPort(QGraphicsEllipseItem)` is a small circle (radius 6 px) placed on block edges.

Each port has:
- `direction`: `"input"` or `"output"`
- `port_type`: signal type string (see Port Types below)
- `connected_wire`: reference to current `WireItem` (or None)

**Port types** (defined in `base_block.py`):

```python
PORT_TYPES = {
    "voltage":    "#e74c3c",   # red
    "current":    "#3498db",   # blue
    "signal":     "#2ecc71",   # green
    "digital":    "#f39c12",   # orange
    "scpi":       "#9b59b6",   # purple
    "trigger":    "#1abc9c",   # teal
    "generic":    "#95a5a6",   # grey
}
```

`WireItem(QGraphicsPathItem)` draws a cubic Bézier between two ports and updates in real time as blocks are moved.

### BLOCK_REGISTRY

To register a new block type so it appears in the Library panel, add an entry:

```python
# canvas.py
from silmulator.gui.blocks.my_device_block import MyDeviceBlock

BLOCK_REGISTRY["My Device"] = MyDeviceBlock
```

---

## Signal Bus

**File:** `silmulator/utils/signals.py`

`SignalBus` is a Qt `QObject` singleton that decouples the GUI from the device layer. Import and use anywhere:

```python
from silmulator.utils.signals import SignalBus

bus = SignalBus.instance()

# Emit
bus.device_command_sent.emit(block_id, "Oscilloscope", "CH1:SCALE 0.5")

# Connect
bus.device_response.connect(my_callback)
```

**Available signals:**

| Signal | Signature | When fired |
|---|---|---|
| `device_command_sent` | `(str, str, str)` | Block sends a SCPI command |
| `device_response` | `(str, str)` | Device returns a response |
| `block_connected` | `(str, str)` | Two blocks wired together |
| `block_disconnected` | `(str, str)` | Wire removed |
| `block_selected` | `(str,)` | Block clicked on canvas |
| `code_parsed` | `(str, list)` | Code Input finishes parsing |
| `waveform_updated` | `(str, np.ndarray)` | Waveform data changed |
| `measurement_updated` | `(str, str, float)` | Measurement value updated |

---

## Code Parser

**File:** `silmulator/code_parser/base_parser.py`

`CodeParser` scans source text for SCPI command strings using regex patterns per language.

```python
from silmulator.code_parser import CodeParser

cp = CodeParser()
commands = cp.parse(source_text, language="c")
# returns list[ParsedCommand]
```

`ParsedCommand` fields:
- `command: str` — the SCPI string
- `line: int` — source line number
- `context: str` — surrounding source line

**Language patterns recognized:**

```
C/C++:
  viWrite(...)             ibwrt(...)
  viQueryf(...)            visa_write(...)
  scpi_send(...)

Python:
  inst.write("...")        inst.query("...")
  rm.open_resource(...).write("...")
  resource.write("...")    resource.query("...")
```

**SCPI heuristic:** A string literal is treated as a SCPI command if it matches:
```
^[A-Z*][A-Z0-9:*?]+
```
(starts with uppercase letter or `*`, contains only SCPI-legal characters)

---

## Waveform Utilities

**File:** `silmulator/utils/waveform.py`

```python
from silmulator.utils.waveform import WaveformType, generate_waveform

wave = generate_waveform(
    wtype=WaveformType.SINE,
    frequency=1000.0,       # Hz
    amplitude=1.0,          # Volts peak
    offset=0.0,             # DC offset (V)
    num_points=10000,
    sample_rate=10e6,       # Sa/s
)
# returns: np.ndarray shape (num_points,)
```

**Available waveform types:**

```python
class WaveformType(Enum):
    SINE              = "SIN"
    SQUARE            = "SQU"
    RAMP              = "RAMP"
    TRIANGLE          = "TRI"
    PULSE             = "PULS"
    NOISE             = "NOIS"
    DC                = "DC"
    EXPONENTIAL_RISE  = "EXP_RISE"
    EXPONENTIAL_FALL  = "EXP_FALL"
    SINC              = "SINC"
    ARBITRARY         = "ARB"
```

---

## Testing

All tests use **pytest** with no external dependencies beyond PyQt6 and numpy.

```bash
# Install dev deps
pip install pytest pytest-qt

# Run all
pytest tests/ -v

# One device
pytest tests/test_oscilloscope.py -v

# One test
pytest tests/test_power_supply.py::test_voltage_set -v

# With coverage
pip install pytest-cov
pytest tests/ --cov=silmulator --cov-report=html
```

**Test structure:**

```python
# tests/test_power_supply.py example
import pytest
from silmulator.devices import PowerSupply

@pytest.fixture
def psu():
    return PowerSupply()

def test_voltage_set(psu):
    psu.process("INST:NSEL 1")
    psu.process("VOLT 3.3")
    assert float(psu.process("VOLT?")) == pytest.approx(3.3)

def test_output_on(psu):
    psu.process("INST:NSEL 1")
    psu.process("VOLT 5.0; OUTP ON")
    assert psu.process("OUTP?") == "1"
```

---

## Build System

### Python package

```bash
pip install -e .                      # Editable install
pip install build && python -m build  # Build wheel + sdist
```

### C++ extension

```bash
# Configure
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release \
         -DCMAKE_POSITION_INDEPENDENT_CODE=ON

# Build
make -j$(nproc)

# Install
cp scpi_core*.so ../silmulator/scpi/
```

### PyInstaller executable

```bash
# macOS / Linux
./build_installer.sh

# Windows
build_installer.bat

# Output in dist/Silmulator/
```

### Docker images

```bash
# Production
docker build -t silmulator:latest .

# Development
docker build -f Dockerfile.dev -t silmulator:dev .
```

---

## Adding a New Device (Step-by-Step)

See `docs/adding_blocks.md` for the full walkthrough with a complete example. Summary:

1. Create `silmulator/devices/my_device.py` inheriting `BaseDevice`.
2. Register handlers in `_setup_handlers()`.
3. Implement `_do_reset()`.
4. Export from `silmulator/devices/__init__.py`.
5. Write tests in `tests/test_my_device.py`.
6. Create `silmulator/gui/blocks/my_device_block.py` (see below).
7. Register in `BLOCK_REGISTRY` in `canvas.py`.

---

## Adding a New GUI Block (Step-by-Step)

See `docs/adding_blocks.md` for full detail. Summary:

1. Create `silmulator/gui/blocks/my_device_block.py` inheriting `BaseBlock`.
2. Implement `device_type`, `_create_device()`, `_build_ports()`.
3. Add custom context menu items if needed.
4. Import and register in `canvas.py → BLOCK_REGISTRY`.

---

## Coding Conventions

| Convention | Rule |
|---|---|
| Python style | PEP 8, 4-space indents, max line 100 |
| Type hints | All public methods annotated |
| Docstrings | Doxygen-style for C++; brief one-liners for Python |
| SCPI paths | Uppercase, colon-separated, both long and short forms registered |
| Dataclasses | Used for all device state groupings |
| Qt signals | Defined in `SignalBus`; never emit directly from device layer |
| Tests | One test function per behavior; use `pytest.approx` for floats |
| Comments | Only when the "why" is non-obvious |

---

*Silmulator v1.0.0 — developer guide*

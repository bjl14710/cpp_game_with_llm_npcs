# Adding New Hardware Blocks to Silmulator

This guide walks through adding a brand-new simulated instrument end-to-end:
from the SCPI device class to the GUI block and the Library panel entry.

The example instrument is a **DC Electronic Load** (e.g., Keysight 63600 series) with voltage, current, and power inputs, and a single SCPI port.

---

## Overview of what you will create

| File | Purpose |
|---|---|
| `silmulator/devices/dc_load.py` | SCPI simulator class |
| `silmulator/gui/blocks/dc_load_block.py` | Canvas block UI |
| `tests/test_dc_load.py` | pytest tests |
| Updated: `silmulator/devices/__init__.py` | Export the new class |
| Updated: `silmulator/gui/canvas.py` | Register the new block |

---

## Step 1 — Create the device class

Create `silmulator/devices/dc_load.py`:

```python
"""DC Electronic Load simulator — Keysight 63600-series compatible."""
from __future__ import annotations
from dataclasses import dataclass, field
from silmulator.devices.base_device import BaseDevice
from silmulator.scpi.parser import ScpiCommand


@dataclass
class DcLoadState:
    """Complete state for one DC Load channel."""
    mode: str = "CC"          # CC / CV / CR / CP
    current: float = 0.0      # Constant-current setpoint (A)
    voltage: float = 0.0      # Constant-voltage setpoint (V)
    resistance: float = 1e6   # Constant-resistance setpoint (Ω)
    power: float = 0.0        # Constant-power setpoint (W)
    enabled: bool = False
    # Measured (simulated) values
    meas_voltage: float = 0.0
    meas_current: float = 0.0
    meas_power: float = 0.0
    # Protection
    ovp: float = 80.0
    ocp: float = 30.0
    opp: float = 200.0
    ovp_tripped: bool = False
    ocp_tripped: bool = False
    opp_tripped: bool = False


class DcLoad(BaseDevice):
    """
    Simulates a DC electronic load.

    Supports CC (constant current), CV (constant voltage),
    CR (constant resistance), and CP (constant power) modes.

    Compatible with Keysight 63600-series SCPI command set.
    """

    DEVICE_ID = "KEYSIGHT TECHNOLOGIES,63600,SN000001,1.0.0"

    def __init__(self) -> None:
        super().__init__()
        self.state = DcLoadState()
        self._setup_handlers()

    # ------------------------------------------------------------------
    # BaseDevice interface
    # ------------------------------------------------------------------

    def _do_reset(self) -> None:
        self.state = DcLoadState()

    def _setup_handlers(self) -> None:
        # Identification
        self._register("*IDN", self._idn)

        # Mode
        self._register_aliases(["MODE", "FUNC"], self._mode_set)
        self._register_aliases(["MODE?", "FUNC?"], self._mode_query)

        # Constant-current
        self._register_aliases(
            ["CURR", "CURRENT", "SOURCE:CURR", "SOURCE:CURRENT"],
            self._curr_set,
        )
        self._register_aliases(
            ["CURR?", "CURRENT?", "SOURCE:CURR?", "SOURCE:CURRENT?"],
            self._curr_query,
        )

        # Constant-voltage
        self._register_aliases(
            ["VOLT", "VOLTAGE", "SOURCE:VOLT", "SOURCE:VOLTAGE"],
            self._volt_set,
        )
        self._register_aliases(
            ["VOLT?", "VOLTAGE?", "SOURCE:VOLT?", "SOURCE:VOLTAGE?"],
            self._volt_query,
        )

        # Constant-resistance
        self._register_aliases(["RES", "RESISTANCE"], self._res_set)
        self._register_aliases(["RES?", "RESISTANCE?"], self._res_query)

        # Constant-power
        self._register_aliases(["POW", "POWER"], self._pow_set)
        self._register_aliases(["POW?", "POWER?"], self._pow_query)

        # Output enable
        self._register_aliases(["INP", "INPUT"], self._input_set)
        self._register_aliases(["INP?", "INPUT?"], self._input_query)

        # Measurements
        self._register("MEAS:VOLT?", self._meas_volt)
        self._register("MEAS:CURR?", self._meas_curr)
        self._register("MEAS:POW?", self._meas_pow)
        self._register("MEASURE:VOLTAGE?", self._meas_volt)
        self._register("MEASURE:CURRENT?", self._meas_curr)
        self._register("MEASURE:POWER?", self._meas_pow)

        # Protection
        self._register("VOLT:PROT", self._ovp_set)
        self._register("VOLT:PROT?", self._ovp_query)
        self._register("CURR:PROT", self._ocp_set)
        self._register("CURR:PROT?", self._ocp_query)
        self._register("POW:PROT", self._opp_set)
        self._register("POW:PROT?", self._opp_query)
        self._register("VOLT:PROT:TRIP?", self._ovp_trip)
        self._register("CURR:PROT:TRIP?", self._ocp_trip)
        self._register("POW:PROT:TRIP?", self._opp_trip)
        self._register("PROT:CLE", self._prot_clear)

    # ------------------------------------------------------------------
    # Handlers
    # ------------------------------------------------------------------

    def _idn(self, cmd: ScpiCommand) -> str:
        return self.DEVICE_ID

    def _mode_set(self, cmd: ScpiCommand) -> str:
        mode = cmd.params[0].upper()
        if mode not in ("CC", "CV", "CR", "CP"):
            self._push_error(-224, "Illegal parameter value")
            return ""
        self.state.mode = mode
        return ""

    def _mode_query(self, cmd: ScpiCommand) -> str:
        return self.state.mode

    def _curr_set(self, cmd: ScpiCommand) -> str:
        self.state.current = float(cmd.params[0])
        return ""

    def _curr_query(self, cmd: ScpiCommand) -> str:
        return f"{self.state.current:.6E}"

    def _volt_set(self, cmd: ScpiCommand) -> str:
        self.state.voltage = float(cmd.params[0])
        return ""

    def _volt_query(self, cmd: ScpiCommand) -> str:
        return f"{self.state.voltage:.6E}"

    def _res_set(self, cmd: ScpiCommand) -> str:
        self.state.resistance = float(cmd.params[0])
        return ""

    def _res_query(self, cmd: ScpiCommand) -> str:
        return f"{self.state.resistance:.6E}"

    def _pow_set(self, cmd: ScpiCommand) -> str:
        self.state.power = float(cmd.params[0])
        return ""

    def _pow_query(self, cmd: ScpiCommand) -> str:
        return f"{self.state.power:.6E}"

    def _input_set(self, cmd: ScpiCommand) -> str:
        val = cmd.params[0].upper()
        self.state.enabled = val in ("ON", "1")
        return ""

    def _input_query(self, cmd: ScpiCommand) -> str:
        return "1" if self.state.enabled else "0"

    def _meas_volt(self, cmd: ScpiCommand) -> str:
        return f"{self.state.meas_voltage:.6E}"

    def _meas_curr(self, cmd: ScpiCommand) -> str:
        return f"{self.state.meas_current:.6E}"

    def _meas_pow(self, cmd: ScpiCommand) -> str:
        return f"{self.state.meas_power:.6E}"

    def _ovp_set(self, cmd: ScpiCommand) -> str:
        self.state.ovp = float(cmd.params[0])
        return ""

    def _ovp_query(self, cmd: ScpiCommand) -> str:
        return f"{self.state.ovp:.6E}"

    def _ocp_set(self, cmd: ScpiCommand) -> str:
        self.state.ocp = float(cmd.params[0])
        return ""

    def _ocp_query(self, cmd: ScpiCommand) -> str:
        return f"{self.state.ocp:.6E}"

    def _opp_set(self, cmd: ScpiCommand) -> str:
        self.state.opp = float(cmd.params[0])
        return ""

    def _opp_query(self, cmd: ScpiCommand) -> str:
        return f"{self.state.opp:.6E}"

    def _ovp_trip(self, cmd: ScpiCommand) -> str:
        return "1" if self.state.ovp_tripped else "0"

    def _ocp_trip(self, cmd: ScpiCommand) -> str:
        return "1" if self.state.ocp_tripped else "0"

    def _opp_trip(self, cmd: ScpiCommand) -> str:
        return "1" if self.state.opp_tripped else "0"

    def _prot_clear(self, cmd: ScpiCommand) -> str:
        self.state.ovp_tripped = False
        self.state.ocp_tripped = False
        self.state.opp_tripped = False
        return ""

    # ------------------------------------------------------------------
    # Test injection helpers
    # ------------------------------------------------------------------

    def inject(self, voltage: float, current: float) -> None:
        """Inject simulated measured values (for deterministic testing)."""
        self.state.meas_voltage = voltage
        self.state.meas_current = current
        self.state.meas_power = voltage * current
```

---

## Step 2 — Export from the devices package

Edit `silmulator/devices/__init__.py` and add:

```python
from silmulator.devices.dc_load import DcLoad
```

---

## Step 3 — Write tests

Create `tests/test_dc_load.py`:

```python
"""Tests for the DC Electronic Load simulator."""
import pytest
from silmulator.devices import DcLoad

@pytest.fixture
def load():
    return DcLoad()

def test_identification(load):
    assert "63600" in load.process("*IDN?")

def test_mode_cc(load):
    load.process("MODE CC")
    assert load.process("MODE?") == "CC"

def test_mode_invalid(load):
    load.process("MODE XYZ")
    err = load.process("SYST:ERR?")
    assert "-224" in err

def test_current_set(load):
    load.process("CURR 10.5")
    assert float(load.process("CURR?")) == pytest.approx(10.5)

def test_voltage_set(load):
    load.process("VOLT 48.0")
    assert float(load.process("VOLT?")) == pytest.approx(48.0)

def test_input_enable(load):
    load.process("INP ON")
    assert load.process("INP?") == "1"

def test_measurement_injection(load):
    load.inject(voltage=12.0, current=2.5)
    assert float(load.process("MEAS:VOLT?")) == pytest.approx(12.0)
    assert float(load.process("MEAS:CURR?")) == pytest.approx(2.5)
    assert float(load.process("MEAS:POW?")) == pytest.approx(30.0)

def test_reset(load):
    load.process("CURR 5.0")
    load.process("*RST")
    assert float(load.process("CURR?")) == pytest.approx(0.0)
```

---

## Step 4 — Create the GUI block

Create `silmulator/gui/blocks/dc_load_block.py`:

```python
"""GUI block for the DC Electronic Load instrument."""
from __future__ import annotations
from PyQt6.QtGui import QColor
from silmulator.devices.dc_load import DcLoad
from silmulator.gui.blocks.base_block import BaseBlock, PORT_TYPES


class DcLoadBlock(BaseBlock):
    """
    Canvas representation of a DC Electronic Load.

    Ports:
        - voltage_in  (input,  voltage)   — source voltage sense
        - current_in  (input,  current)   — source current sense
        - scpi        (input,  scpi)      — SCPI command bus
    """

    BLOCK_COLOR = QColor("#c0392b")   # dark red — matches "load/sink" convention
    BLOCK_LABEL = "DC Electronic Load"
    BLOCK_BADGE = "LOAD"

    # ------------------------------------------------------------------
    # BaseBlock contract
    # ------------------------------------------------------------------

    @property
    def device_type(self) -> str:
        return "DC Load"

    def _create_device(self) -> DcLoad:
        return DcLoad()

    def _build_ports(self) -> None:
        # Input ports on the left edge
        self._add_port("voltage_in", "input",  PORT_TYPES["voltage"],  side="left",  offset=0.25)
        self._add_port("current_in", "input",  PORT_TYPES["current"],  side="left",  offset=0.75)
        # SCPI bus on the top edge
        self._add_port("scpi",       "input",  PORT_TYPES["scpi"],     side="top",   offset=0.5)

    # ------------------------------------------------------------------
    # Optional: custom context menu entries
    # ------------------------------------------------------------------

    def _extra_context_actions(self) -> list[tuple[str, callable]]:
        return [
            ("Set CC mode",  lambda: self.device.process("MODE CC")),
            ("Set CV mode",  lambda: self.device.process("MODE CV")),
            ("Enable input", lambda: self.device.process("INP ON")),
        ]
```

---

## Step 5 — Register in BLOCK_REGISTRY

Edit `silmulator/gui/canvas.py`:

```python
# Add import near the top
from silmulator.gui.blocks.dc_load_block import DcLoadBlock

# Add to BLOCK_REGISTRY dict
BLOCK_REGISTRY: dict[str, type[BaseBlock]] = {
    "Oscilloscope":       OscilloscopeBlock,
    "Power Supply":       PowerSupplyBlock,
    "Function Generator": FunctionGeneratorBlock,
    "Multimeter":         MultimeterBlock,
    "MSP430":             MSP430Block,
    "Code Input":         CodeInputBlock,
    "DC Load":            DcLoadBlock,    # ← new entry
}
```

---

## Common Port Types Reference

When calling `self._add_port(name, direction, port_type, side, offset)`:

| `port_type` constant | Color | Use for |
|---|---|---|
| `PORT_TYPES["voltage"]` | Red | Voltage sense / source outputs |
| `PORT_TYPES["current"]` | Blue | Current sense / source outputs |
| `PORT_TYPES["signal"]` | Green | Analog signal (from function generator, oscilloscope trigger) |
| `PORT_TYPES["digital"]` | Orange | GPIO, digital I/O, logic signals |
| `PORT_TYPES["scpi"]` | Purple | SCPI command bus (standard for all instruments) |
| `PORT_TYPES["trigger"]` | Teal | Trigger in/out between instruments |
| `PORT_TYPES["generic"]` | Grey | Catch-all / unknown signal type |

### Port sides and offsets

- `side`: `"left"`, `"right"`, `"top"`, `"bottom"`
- `offset`: `0.0` to `1.0` — fractional position along the edge

Example for a PSU-like block with 3 output voltage channels on the right:

```python
self._add_port("ch1_v", "output", PORT_TYPES["voltage"], side="right", offset=0.2)
self._add_port("ch2_v", "output", PORT_TYPES["voltage"], side="right", offset=0.5)
self._add_port("ch3_v", "output", PORT_TYPES["voltage"], side="right", offset=0.8)
self._add_port("scpi",  "input",  PORT_TYPES["scpi"],    side="top",   offset=0.5)
```

---

## Common Input/Output Patterns by Instrument Type

### Voltage Source (power supply, bench supply)
- Outputs: `voltage` (one per channel), `current` (one per channel)
- Input: `scpi`

### Current Source
- Outputs: `current`, `voltage` (sense)
- Input: `scpi`

### Function / Arbitrary Waveform Generator
- Outputs: `signal` (one per channel), `trigger` (sync output)
- Input: `scpi`, `trigger` (external trigger in)

### Oscilloscope / Data Acquisition
- Inputs: `signal` (one per channel), `trigger` (external trigger)
- Input: `scpi`
- No outputs (reads only)

### Digital Multimeter
- Inputs: `voltage`, `current`, `generic` (test leads)
- Input: `scpi`

### Electronic Load
- Inputs: `voltage`, `current` (from DUT)
- Input: `scpi`

### Microcontroller
- Inputs/Outputs: `digital` (GPIO), `signal` (ADC), `scpi`

### Network Analyzer / Spectrum Analyzer
- Inputs: `signal` (RF in), `trigger`
- Outputs: `signal` (RF out / reference)
- Input: `scpi`

---

## Checklist

- [ ] `silmulator/devices/my_device.py` — inherits `BaseDevice`, implements `_do_reset` and `_setup_handlers`
- [ ] `silmulator/devices/__init__.py` — exports the new class
- [ ] `tests/test_my_device.py` — covers set, query, reset, inject
- [ ] `silmulator/gui/blocks/my_device_block.py` — inherits `BaseBlock`, implements `device_type`, `_create_device`, `_build_ports`
- [ ] `silmulator/gui/canvas.py` — adds entry to `BLOCK_REGISTRY`
- [ ] Run `pytest tests/` — all tests pass

---

*Silmulator v1.0.0 — block extension guide*

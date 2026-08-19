# Silmulator User Guide

**Version 1.0.0 — SIL/HIL/CIL Hardware Simulator**

---

## Table of Contents

1. [Introduction](#introduction)
2. [System Requirements](#system-requirements)
3. [Installation](#installation)
   - [Option A: pip install](#option-a-pip-install)
   - [Option B: Docker (Web Browser)](#option-b-docker-web-browser)
   - [Option C: Docker (Native Display)](#option-c-docker-native-display)
   - [Option D: From Source](#option-d-from-source)
4. [Quick Start](#quick-start)
5. [Application Layout](#application-layout)
6. [Virtual Hardware Reference](#virtual-hardware-reference)
   - [Oscilloscope — Tektronix MSO54](#oscilloscope--tektronix-mso54)
   - [Power Supply — Keysight E36312A](#power-supply--keysight-e36312a)
   - [Function Generator — Keysight 33522B](#function-generator--keysight-33522b)
   - [Multimeter — Keysight 34401A](#multimeter--keysight-34401a)
   - [Microcontroller — TI MSP430FR5994](#microcontroller--ti-msp430fr5994)
   - [Code Input Block](#code-input-block)
7. [Building a Simulation](#building-a-simulation)
8. [SCPI Console](#scpi-console)
9. [Working with Source Code](#working-with-source-code)
10. [Saving and Loading Layouts](#saving-and-loading-layouts)
11. [Keyboard Shortcuts](#keyboard-shortcuts)
12. [Troubleshooting](#troubleshooting)

---

## Introduction

Silmulator is a **Software-in-the-Loop (SIL) / Hardware-in-the-Loop (HIL) / Code-in-the-Loop (CIL)** virtual lab bench. Instead of wiring up physical instruments, you place virtual blocks on a canvas, connect them with wires, and interact with them using the same **SCPI** (Standard Commands for Programmable Instruments) that your real measurement code already sends.

### What it replaces

| Physical item | Virtual equivalent in Silmulator |
|---|---|
| Tektronix MSO54 oscilloscope | Oscilloscope block |
| Keysight E36312A triple-output PSU | Power Supply block |
| Keysight 33522B dual-channel AWG | Function Generator block |
| Keysight 34401A 6½-digit DMM | Multimeter block |
| TI MSP430FR5994 microcontroller | MSP430 block |
| VISA/GPIB cable | Wire connection on canvas |

### Why use it

- **No hardware needed** — debug firmware and test scripts at your desk.
- **Deterministic results** — inject exact voltage/waveform values for reproducible tests.
- **Fast iteration** — change a SCPI command, see the response instantly; no reboots.
- **Team-friendly** — check Docker image out of version control, everyone runs the same bench.

---

## System Requirements

### Native installation

| Component | Minimum |
|---|---|
| OS | macOS 12+, Windows 10, or Ubuntu 20.04+ |
| Python | 3.9 or newer |
| RAM | 512 MB |
| Disk | 300 MB |
| Display | Any (1024×768 minimum recommended) |

### Docker installation

| Component | Minimum |
|---|---|
| Docker Desktop | 4.0+ |
| RAM | 1 GB allocated to Docker |
| Browser (web UI) | Chrome, Firefox, Safari, Edge |
| Disk | 2 GB |

---

## Installation

### Option A: pip install

```bash
# 1 — Clone the repository
git clone <repository-url>
cd Silmulator

# 2 — Create a virtual environment (recommended)
python3 -m venv .venv
source .venv/bin/activate        # macOS/Linux
# .venv\Scripts\activate         # Windows

# 3 — Install the package
pip install -e .

# 4 — Launch
silmulator
```

Or run directly without installing:

```bash
pip install PyQt6 numpy scipy
python main.py
```

### Option B: Docker (Web Browser)

No display setup needed — the application streams to your browser via NoVNC.

```bash
# Build and start
docker compose up -d

# Open in browser
open http://localhost:6080/vnc.html
```

The default VNC password is `silmulator`. To change it:

```bash
VNC_PASSWORD=mysecret docker compose up -d
```

To stop:

```bash
docker compose down
```

### Option C: Docker (Native Display)

Renders in a native window on your host. Requires an X11 server (XQuartz on macOS, standard on Linux).

```bash
# macOS: start XQuartz first, then allow connections
xhost +localhost

# Start development container
docker compose -f docker-compose.dev.yml up
```

### Option D: From Source (with C++ acceleration)

Builds the optional high-performance C++ SCPI parser.

```bash
# Prerequisites: CMake ≥ 3.16, pybind11, C++17 compiler

# macOS
brew install cmake pybind11

# Ubuntu/Debian
apt install cmake python3-dev pybind11-dev g++

# Build
./build_cpp.sh

# Install Python package
pip install -e .
silmulator
```

---

## Quick Start

1. **Launch** the application with `silmulator` or `python main.py`.
2. In the **Library** panel on the left, **double-click "Power Supply"** to add it to the canvas.
3. **Double-click "Multimeter"** to add a multimeter.
4. **Click the output port** (small circle) on the Power Supply block and **drag to the input port** on the Multimeter.
5. **Right-click the Power Supply** → **Send SCPI command** → type `VOLT 5.0` → press Enter.
6. **Right-click the Power Supply** → **Send SCPI command** → type `OUTP ON`.
7. **Right-click the Multimeter** → **Send SCPI command** → type `MEAS:VOLT:DC?`.
8. The response `+5.00000000E+00` appears in the Console at the bottom.

---

## Application Layout

```
┌─────────────────────────────────────────────────────────────┐
│  File   Edit   View   Help                                  │  ← Menu bar
├──────────────┬──────────────────────────────────────────────┤
│              │                                              │
│   Library    │              Canvas                          │
│   Panel      │   [PSU Block]──[Multimeter Block]            │
│              │                                              │
│  ○ Oscilloscope        drag & drop blocks here             │
│  ○ Power Supply        connect ports with wires            │
│  ○ Func. Generator     zoom: scroll wheel                  │
│  ○ Multimeter          pan: middle-mouse drag              │
│  ○ MSP430              select all: Ctrl+A                  │
│  ○ Code Input                                              │
│              │                                              │
├──────────────┴──────────────────────────────────────────────┤
│  SCPI Console                                               │
│  > VOLT 5.0                                                 │
│  < OK                                                       │
│  > MEAS:VOLT:DC?                                            │
│  < +5.00000000E+00                                          │
└─────────────────────────────────────────────────────────────┘
```

**Library Panel** — lists all available virtual instrument types. Double-click to place on canvas.

**Canvas** — the main work area. Blocks can be freely moved. Ports connect with wires.

**SCPI Console** — send raw SCPI commands to the currently selected block and see responses.

---

## Virtual Hardware Reference

### Oscilloscope — Tektronix MSO54

The oscilloscope simulates a 4-channel mixed-signal Tektronix MSO54 with full SCPI command coverage.

#### Channels

| Channel | Name |
|---|---|
| Analog 1 | CH1 |
| Analog 2 | CH2 |
| Analog 3 | CH3 |
| Analog 4 | CH4 |

#### Key SCPI Commands

```scpi
# Channel setup
CH1:SCALE 0.5          # 500 mV/division
CH1:POSITION 0.0       # Vertical position
CH1:COUPLING DC        # AC / DC / GND
CH1:BANDWIDTH 20E6     # Bandwidth limit (Hz)

# Timebase
HOR:SCALE 1E-3         # 1 ms/division
HOR:POSITION 0.0       # Horizontal trigger position (%)
HOR:RECORDLENGTH 10000 # Samples per acquisition

# Trigger
TRIG:A:TYPE EDGE
TRIG:A:EDGE:SOURCE CH1
TRIG:A:EDGE:SLOPE RISE  # RISE / FALL / EITHER
TRIG:A:LEVEL 1.5        # Trigger threshold (V)
TRIG:A:MODE AUTO        # AUTO / NORMAL / SINGLE

# Acquisition
ACQ:STATE 1             # Run (1) / Stop (0)
ACQ:MODE SAMPLE         # SAMPLE / AVERAGE / PEAKDETECT / HIRES
ACQ:NUMAVG 16           # Averages when mode=AVERAGE

# Measurements
MEASUREMENT:MEAS1:TYPE FREQUENCY
MEASUREMENT:MEAS1:SOURCE CH1
MEASUREMENT:MEAS1:VALUE?   # Query measured value

# Waveform transfer
CURVE?                  # Returns waveform data
DATA:SOURCE CH1
DATA:ENC ASCII          # ASCII / BINARY
WFMPRE?                 # Waveform preamble (scale, offset, etc.)

# Common IEEE 488.2
*IDN?                   # Returns instrument identification
*RST                    # Reset to defaults
*CLS                    # Clear status registers
```

#### Available Measurements

`FREQUENCY`, `PERIOD`, `AMPLITUDE`, `RMS`, `MEAN`, `HIGH`, `LOW`, `PK2PK`, `RISE`, `FALL`, `PDUTY`, `NDUTY`

#### Injecting waveforms (Python API)

```python
import numpy as np
from silmulator.devices import Oscilloscope

osc = Oscilloscope()
t = np.linspace(0, 1e-3, 10000)  # 1 ms window
wave = np.sin(2 * np.pi * 1000 * t)  # 1 kHz sine
osc.inject_waveform("CH1", wave)
osc.process("MEASUREMENT:MEAS1:TYPE FREQUENCY")
print(osc.process("MEASUREMENT:MEAS1:VALUE?"))  # → 1000.0
```

---

### Power Supply — Keysight E36312A

Three independently controlled DC output channels. Simulates voltage, current, and protection features.

#### Channels

| Channel | Voltage Max | Current Max |
|---|---|---|
| CH1 | 6 V | 5 A |
| CH2 | 25 V | 1 A |
| CH3 | 25 V | 1 A |

#### Key SCPI Commands

```scpi
# Select channel
INST:NSEL 1             # Select channel 1 (1/2/3)
INST CH1                # Alternate form

# Set output
VOLT 3.3                # Set voltage (V)
CURR 1.0                # Set current limit (A)
OUTP ON                 # Enable output
OUTP OFF                # Disable output

# Combined set
APPL 5.0, 2.0           # APPLY <voltage>,<current> — shorthand

# Measure (reads simulated output)
MEAS:VOLT:DC?           # Measured voltage
MEAS:CURR:DC?           # Measured current
MEAS:POW?               # Measured power (V × I)

# Over-voltage/current protection
VOLT:PROT 6.0           # OVP setpoint (V)
VOLT:PROT:STAT ON       # Enable OVP
CURR:PROT 5.0           # OCP setpoint (A)
CURR:PROT:STAT ON       # Enable OCP
VOLT:PROT:TRIP?         # 1 if OVP has tripped
CURR:PROT:TRIP?         # 1 if OCP has tripped
VOLT:PROT:CLE           # Clear OVP latch
```

---

### Function Generator — Keysight 33522B

Two-channel arbitrary waveform generator. Generates standard functions, modulated signals, bursts, and sweeps.

#### Channels

| Channel | Label |
|---|---|
| 1 | SOURCE1 |
| 2 | SOURCE2 |

#### Key SCPI Commands

```scpi
# Waveform function
SOURCE1:FUNC SIN        # SIN / SQU / RAMP / PULS / NOIS / DC / USER / ARB
SOURCE1:FREQ 1000       # Frequency (Hz)
SOURCE1:VOLT 2.0        # Amplitude peak-to-peak (V)
SOURCE1:VOLT:OFFSET 0   # DC offset (V)
SOURCE1:PHAS 0          # Phase (degrees)

# Square wave duty cycle
SOURCE1:FUNC SQU
SOURCE1:FUNC:SQU:DCYCLE 50  # Duty cycle %

# Pulse parameters
SOURCE1:FUNC PULS
SOURCE1:FUNC:PULS:WIDT 1E-4  # Pulse width (s)
SOURCE1:FUNC:PULS:EDGE 1E-5  # Edge time (s)

# Output control
OUTPUT1 ON              # Enable channel 1
OUTPUT2 ON              # Enable channel 2
OUTPUT1:LOAD INF        # Load impedance (INF / 50)

# Modulation — AM
SOURCE1:AM:STAT ON
SOURCE1:AM:DEPTH 50     # Depth %
SOURCE1:AM:FREQ 100     # Modulation frequency (Hz)

# Sweep
SOURCE1:SWEEP:STAT ON
SOURCE1:FREQ:STAR 100   # Start frequency (Hz)
SOURCE1:FREQ:STOP 10000 # Stop frequency (Hz)
SOURCE1:SWEEP:TIME 1    # Sweep time (s)

# Burst
SOURCE1:BURS:STAT ON
SOURCE1:BURS:NCYC 5     # Cycles per burst
SOURCE1:BURS:MODE TRIG  # TRIG / GATE
```

---

### Multimeter — Keysight 34401A

6½-digit digital multimeter. Supports DC/AC voltage, DC/AC current, resistance, continuity, diode, and temperature.

#### Key SCPI Commands

```scpi
# One-shot measurement (fastest path)
MEAS:VOLT:DC?           # DC voltage (auto-range)
MEAS:VOLT:AC?           # AC voltage (rms)
MEAS:CURR:DC?           # DC current
MEAS:CURR:AC?           # AC current (rms)
MEAS:RES?               # 2-wire resistance
MEAS:FRES?              # 4-wire resistance
MEAS:FREQ?              # Frequency (Hz)
MEAS:CONT?              # Continuity (0 or 1)
MEAS:DIOD?              # Diode forward voltage

# Configure then read (more control)
CONF:VOLT:DC 10, 0.001  # Range=10V, resolution=1mV
READ?                    # Trigger and return one reading
INIT                     # Trigger without returning
FETCH?                   # Retrieve last reading

# Range and resolution
VOLT:DC:RANG 10         # Manual range (V)
VOLT:DC:RANG:AUTO ON    # Auto-range

# Null (REL) subtraction
CALC:FUNC NULL
CALC:STAT ON
CALC:NULL:OFFS 0.005    # Subtract 5 mV from readings

# Statistics
CALC:FUNC AVER
CALC:STAT ON
CALC:AVER:MIN?          # Minimum of running average
CALC:AVER:MAX?          # Maximum of running average
CALC:AVER:AVER?         # Current average

# Limits
CALC:FUNC LIM
CALC:LIM:LOW 4.9
CALC:LIM:UPP 5.1
CALC:LIM:FAIL?          # 0=pass, 1=fail high, -1=fail low
```

#### Injecting values (Python API)

```python
from silmulator.devices import Multimeter

dmm = Multimeter()
dmm.inject("VOLT:DC", 3.296)
print(dmm.process("MEAS:VOLT:DC?"))  # → +3.29600000E+00
```

---

### Microcontroller — TI MSP430FR5994

Full register-level simulation of the MSP430FR5994 FRAM microcontroller. Supports GPIO, ADC, timers, UART, SPI, and I2C.

#### GPIO

```scpi
GPIO:PORT1:DIRECTION 0xFF    # All port 1 pins = output
GPIO:PORT1:OUTPUT 0x01       # Set P1.0 high
GPIO:PORT1:INPUT?            # Read port 1 input register
GPIO:PORT1:REN 0xFF          # Enable pull resistors
GPIO:PORT1:OUT 0xAA          # Set pull direction

# Individual pin
GPIO:PORT2:PIN3:DIR OUT      # P2.3 = output
GPIO:PORT2:PIN3:OUT 1        # P2.3 = high
GPIO:PORT2:PIN3:IN?          # Read P2.3
```

#### ADC (12-bit, 12 channels)

```scpi
ADC:CHANNEL0:INJECT 1.65     # Inject 1.65 V on AIN0
ADC:CONVERT 0                # Perform conversion on channel 0
ADC:CHANNEL0:RAW?            # Read raw 12-bit value (0–4095)
ADC:REFVOLT 3.3              # Set reference voltage
ADC:RESOLUTION 12            # 8 / 10 / 12 bits
ADC:SAMPLERATE 200000        # Sample rate (Hz)
```

#### UART

```scpi
UART:A0:BAUD 115200          # Set baud rate
UART:A0:DATABITS 8           # Data bits
UART:A0:PARITY NONE          # NONE / ODD / EVEN
UART:A0:STOPBITS 1           # 1 or 2
UART:A0:ENABLE ON
UART:A0:TX "Hello"           # Simulate TX
UART:A0:RX?                  # Read RX buffer
```

#### SPI

```scpi
SPI:B0:ENABLE ON
SPI:B0:CLK 1000000           # Clock frequency (Hz)
SPI:B0:CPOL 0                # Clock polarity (0/1)
SPI:B0:CPHA 0                # Clock phase (0/1)
SPI:B0:TRANSFER 0xAB         # Transfer byte, returns received byte
```

#### I2C

```scpi
I2C:B1:ENABLE ON
I2C:B1:ADDR 0x48             # Own address
I2C:B1:FREQ 400000           # Bus frequency (Hz)
I2C:B1:WRITE 0x48, 0x00      # Write to slave address
I2C:B1:READ 0x48, 2          # Read 2 bytes from slave
```

#### Clocks

```scpi
CLK:DCO:FREQ 16000000        # DCO frequency (Hz)
CLK:MCLK:SOURCE DCO          # MCLK source (DCO/LFXT/HFXT)
CLK:SMCLK:DIV 1              # SMCLK divider
CLK:ACLK:SOURCE LFXT         # ACLK source
```

#### Power Modes

```scpi
PM:MODE AM                   # Active mode
PM:MODE LPM0                 # Low-power mode 0
PM:MODE LPM3                 # Low-power mode 3 (deep sleep)
PM:MODE LPM4_5               # Lowest power (only RESET wake)
PM:MODE?                     # Query current mode
```

---

### Code Input Block

The Code Input block analyzes your C, C++, or Python source files and extracts every SCPI command string, then routes them to connected hardware blocks.

#### Supported patterns

**C / C++**
```c
viWrite(session, "VOLT 5.0", 8, &count);
viQueryf(session, "MEAS:VOLT:DC?", "%t", buf);
visa_write(dev, "OUTP ON");
scpi_send(handle, "ACQ:STATE 1");
ibwrt(dev, "TRIG:A:LEVEL 1.5", 16);
```

**Python (pyvisa)**
```python
inst.write("VOLT 5.0")
result = inst.query("MEAS:VOLT:DC?")
rm.open_resource("GPIB0::1::INSTR").write("OUTP ON")
```

#### Usage

1. Double-click "Code Input" in the Library to add the block.
2. Double-click the block to open the code editor.
3. Paste or type your source code.
4. Click **Parse** — the extracted commands appear in the list.
5. Connect the Code Input block to the target hardware block using a wire.
6. Click **Run parsed commands** (or right-click → "Run parsed commands").
7. Watch the Console for responses.

---

## Building a Simulation

### Typical workflow

```
1. Place hardware blocks on canvas
2. Connect blocks with wires
3. Send SCPI commands via:
   a. Right-click menu (individual commands)
   b. SCPI Console (typed commands)
   c. Code Input block (batch from source code)
4. Observe responses in Console
5. (Optional) Inject signal data from Python API
6. Save layout for later
```

### Wire connections

Wires represent **logical SCPI communication links** — they determine which device receives commands from the Code Input block or from connected blocks. They do not carry analog signals in the GUI (signal injection uses the Python API).

To connect two blocks:
1. Hover over a block — small circles (ports) appear on its edges.
2. Click one port and drag to a port on another block.
3. Release — a wire is drawn.

To delete a wire: click it to select, then press `Delete`.

---

## SCPI Console

The console at the bottom of the window accepts commands typed directly. Commands are sent to the **currently selected block** on the canvas.

**Select a block first** (click on it), then type in the console and press `Enter`.

The console shows:
- `>` prefix for commands you send
- `<` prefix for responses from the device
- Error messages in red

---

## Working with Source Code

### Loading a source file

1. Add a **Code Input** block.
2. Double-click it to open the editor.
3. Either paste code directly, or use **File → Open** in the editor dialog to load a `.c`, `.cpp`, or `.py` file.
4. Click **Parse**.

### Routing commands to hardware

The Code Input block routes extracted commands to **whichever hardware block it is wired to**. Connect one wire per target instrument.

### Filtering by device

After parsing, a dropdown lets you filter extracted commands by the SCPI command prefix:
- Commands starting with `CH`, `HOR`, `TRIG`, `ACQ`, `CURVE` → Oscilloscope
- Commands starting with `VOLT`, `CURR`, `OUTP`, `APPL` → Power Supply
- Commands starting with `SOURCE`, `OUTPUT` → Function Generator
- Commands starting with `MEAS`, `CONF`, `CALC` → Multimeter
- Commands starting with `GPIO`, `ADC`, `UART`, `SPI`, `I2C`, `CLK`, `PM` → MSP430

---

## Saving and Loading Layouts

Layouts are saved as `.sil` JSON files that store:
- Block types, positions, and labels
- Wire connections
- Per-block device state (voltages, settings, etc.)
- Code Input content

```
File → Save          Ctrl+S   Save to current file
File → Save As...             Save to new file
File → Open          Ctrl+O   Load a .sil layout
File → New           Ctrl+N   Clear canvas
```

---

## Keyboard Shortcuts

| Shortcut | Action |
|---|---|
| `Ctrl+N` | New canvas |
| `Ctrl+O` | Open layout |
| `Ctrl+S` | Save layout |
| `Ctrl+A` | Select all blocks |
| `Delete` | Delete selected blocks or wires |
| `Ctrl+0` | Reset zoom to 100% |
| `Ctrl+Shift+F` | Fit all blocks in view |
| Middle-mouse drag | Pan canvas |
| Scroll wheel | Zoom in / out |

---

## Troubleshooting

### Application does not start

- Verify Python ≥ 3.9: `python3 --version`
- Check PyQt6 is installed: `python3 -c "import PyQt6; print(PyQt6.QtCore.PYQT_VERSION_STR)"`
- On Linux, install Qt libraries: `apt install libxcb-cursor0 libxcb-xinerama0`
- On macOS with Apple Silicon, use Rosetta or a native arm64 Python build.

### "SCPI Error -113: Undefined header"

The command string does not match any registered handler. Check:
- Spelling and capitalization (SCPI is case-insensitive but spelling must match the standard).
- That the command belongs to the selected device — e.g., sending `ADC:CONVERT 0` to an Oscilloscope will fail.

### Docker — browser shows blank screen

- Give the container a few extra seconds to start the VNC server.
- Refresh the browser page.
- Check logs: `docker compose logs -f`

### Docker — VNC password prompt loops

- The default password is `silmulator`. If you set `VNC_PASSWORD`, use that value.
- To reset: `docker compose down -v && docker compose up -d`

### Waveform curve returns zeros

- The oscilloscope only generates waveform data after `ACQ:STATE 1` (run) and at least one call to `inject_waveform()` or after a connected Function Generator triggers data.
- Use the Python API to inject a numpy array directly.

### C++ extension not loading

- Verify the build succeeded: `ls silmulator/scpi/scpi_core*.so`
- If missing, the pure-Python parser is used automatically — performance impact only.

---

*Silmulator version 1.0.0 — documentation generated 2026-05-09*

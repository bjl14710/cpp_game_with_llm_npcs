# Silmulator Architecture & Diagrams

This document provides conceptual diagrams of how Silmulator works, from
user code input all the way to virtual instrument responses.

All diagrams use [Mermaid](https://mermaid.js.org/) syntax and can be rendered in
GitHub, GitLab, VS Code (with the Mermaid extension), or any Mermaid-compatible viewer.

---

## 1. System Overview

```mermaid
graph TB
    subgraph User["User"]
        SRC["Source Code\n(C/C++/Python)"]
        CMD["Manual SCPI Command"]
        GUI_ACTION["GUI Interaction\n(drag, drop, connect)"]
    end

    subgraph GUI["GUI Layer — PyQt6"]
        MW["MainWindow"]
        LIB["Block Library\nPanel"]
        CANVAS["Canvas\n(CanvasScene / CanvasView)"]
        CON["SCPI Console"]
        CODE_BLOCK["Code Input Block"]
        OSC_BLOCK["Oscilloscope Block"]
        PSU_BLOCK["Power Supply Block"]
        FGEN_BLOCK["Function Generator Block"]
        DMM_BLOCK["Multimeter Block"]
        MCU_BLOCK["MSP430 Block"]
    end

    subgraph DEVICE["Device Layer — Python"]
        OSC["Oscilloscope\n(Tektronix MSO54)"]
        PSU["Power Supply\n(Keysight E36312A)"]
        FGEN["Function Generator\n(Keysight 33522B)"]
        DMM["Multimeter\n(Keysight 34401A)"]
        MCU["MSP430\n(TI MSP430FR5994)"]
    end

    subgraph PARSER["Parser Layer"]
        PY_PARSER["Python SCPI Parser\n(always available)"]
        CPP_PARSER["C++ SCPI Parser\n(optional, faster)"]
        CODE_PARSER["Code Parser\n(extracts SCPI\nfrom source)"]
    end

    SRC --> CODE_BLOCK
    CMD --> CON
    GUI_ACTION --> CANVAS

    MW --> LIB
    MW --> CANVAS
    MW --> CON

    LIB -->|"double-click"| CANVAS
    CODE_BLOCK --> CODE_PARSER
    CODE_PARSER -->|"extracted commands"| CODE_BLOCK
    CODE_BLOCK -->|"SCPI string"| OSC_BLOCK
    CODE_BLOCK -->|"SCPI string"| PSU_BLOCK
    CON -->|"typed command"| OSC_BLOCK
    CON -->|"typed command"| PSU_BLOCK

    OSC_BLOCK --> OSC
    PSU_BLOCK --> PSU
    FGEN_BLOCK --> FGEN
    DMM_BLOCK --> DMM
    MCU_BLOCK --> MCU

    OSC -->|"process()"| PY_PARSER
    PSU -->|"process()"| PY_PARSER
    PY_PARSER -.->|"if available"| CPP_PARSER

    OSC -->|"response"| OSC_BLOCK
    PSU -->|"response"| PSU_BLOCK
    OSC_BLOCK -->|"display"| CON
    PSU_BLOCK -->|"display"| CON
```

---

## 2. SCPI Command Lifecycle

```mermaid
sequenceDiagram
    participant User
    participant GUI as GUI Block / Console
    participant Device as BaseDevice
    participant Parser as ScpiParser
    participant Handler as SCPI Handler

    User->>GUI: type "CH1:SCALE 0.5"
    GUI->>Device: process("CH1:SCALE 0.5")
    Device->>Parser: parse("CH1:SCALE 0.5")
    Parser-->>Device: [ScpiCommand(path="CH1:SCALE", is_query=False, params=["0.5"])]
    Device->>Device: normalize path → look up _handlers["CH1:SCALE"]
    Device->>Handler: _ch_scale(cmd)
    Handler->>Handler: channels[1].scale = 0.5
    Handler-->>Device: "" (empty — set command)
    Device-->>GUI: ""
    GUI->>GUI: display "" or "OK" in console

    User->>GUI: type "CH1:SCALE?"
    GUI->>Device: process("CH1:SCALE?")
    Device->>Parser: parse("CH1:SCALE?")
    Parser-->>Device: [ScpiCommand(path="CH1:SCALE", is_query=True, params=[])]
    Device->>Handler: _ch_scale(cmd)
    Handler-->>Device: "5.00000000E-01"
    Device-->>GUI: "5.00000000E-01"
    GUI->>GUI: display "+5.00E-01" in console
```

---

## 3. Data Flow: Code → Blocks → Output

```mermaid
flowchart LR
    subgraph INPUT["Input"]
        direction TB
        C["C Source\nviWrite(h, 'VOLT 5.0')"]
        CPP["C++ Source\nscpi_send(h, 'OUTP ON')"]
        PY["Python Source\ninst.write('MEAS:VOLT:DC?')"]
    end

    subgraph PARSE["Code Parser"]
        CP["CodeParser\n.parse(text, language)"]
        PC["ParsedCommand list\n[cmd, line, context]"]
        C --> CP
        CPP --> CP
        PY --> CP
        CP --> PC
    end

    subgraph ROUTE["Routing (Canvas)"]
        direction TB
        CB["Code Input Block"]
        WIRE1["Wire → PSU"]
        WIRE2["Wire → DMM"]
        PC --> CB
        CB --> WIRE1
        CB --> WIRE2
    end

    subgraph DEVICES["Device Layer"]
        direction TB
        PSU_D["PowerSupply.process()\n→ VOLT=5.0, OUTP=ON"]
        DMM_D["Multimeter.process()\n→ READ? → +5.000E+00"]
        WIRE1 --> PSU_D
        WIRE2 --> DMM_D
    end

    subgraph OUTPUT["Output"]
        direction TB
        CON["Console\n< +5.000E+00"]
        LOG["Command Log\n[block_id, cmd, response]"]
        SIG["SignalBus events\ndevice_response emitted"]
        PSU_D --> CON
        DMM_D --> CON
        PSU_D --> LOG
        DMM_D --> SIG
    end
```

---

## 4. Class Hierarchy

```mermaid
classDiagram
    class BaseDevice {
        +process(raw: str) str
        +reset()
        #_register(path, handler)
        #_register_aliases(paths, handler)
        #_push_error(code, msg)
        #_pop_error() tuple
        #_do_reset()*
        #_setup_handlers()*
        -_handlers: dict
        -_error_queue: list
        -_parser: ScpiParser
    }

    class Oscilloscope {
        +channels: dict[int, ChannelState]
        +acquisition: AcqState
        +trigger: TriggerState
        +inject_waveform(source, array)
    }

    class PowerSupply {
        +channels: dict[int, ChannelState]
        +selected_channel: int
    }

    class FunctionGenerator {
        +channels: dict[int, SourceState]
    }

    class Multimeter {
        +function: str
        +inject(function, value)
    }

    class MSP430 {
        +ports: dict[int, PortState]
        +adc: AdcState
        +timers: dict
        +uart: dict
        +spi: dict
        +i2c: dict
    }

    BaseDevice <|-- Oscilloscope
    BaseDevice <|-- PowerSupply
    BaseDevice <|-- FunctionGenerator
    BaseDevice <|-- Multimeter
    BaseDevice <|-- MSP430

    class BaseBlock {
        +device: BaseDevice
        +device_type: str*
        #_create_device() BaseDevice*
        #_build_ports()*
        #_add_port(name, dir, type, side, offset)
        +contextMenuEvent()
    }

    class OscilloscopeBlock {
        +device: Oscilloscope
    }
    class PowerSupplyBlock {
        +device: PowerSupply
    }
    class FunctionGeneratorBlock {
        +device: FunctionGenerator
    }
    class MultimeterBlock {
        +device: Multimeter
    }
    class MSP430Block {
        +device: MSP430
    }
    class CodeInputBlock {
        +parser: CodeParser
        +parse()
        +run_parsed()
    }

    BaseBlock <|-- OscilloscopeBlock
    BaseBlock <|-- PowerSupplyBlock
    BaseBlock <|-- FunctionGeneratorBlock
    BaseBlock <|-- MultimeterBlock
    BaseBlock <|-- MSP430Block
    BaseBlock <|-- CodeInputBlock

    OscilloscopeBlock --> Oscilloscope
    PowerSupplyBlock --> PowerSupply
    FunctionGeneratorBlock --> FunctionGenerator
    MultimeterBlock --> Multimeter
    MSP430Block --> MSP430
```

---

## 5. SCPI Parser Internals

```mermaid
flowchart TD
    RAW["Raw input string\n'CH1:SCALE 0.5; TRIG:A:LEVEL?'"]

    SPLIT["split_compound()\nsplit on ';' respecting quotes"]

    CMD1["'CH1:SCALE 0.5'"]
    CMD2["'TRIG:A:LEVEL?'"]

    TOK1["tokenize()\n[HEADER:'CH1:SCALE', PARAM:'0.5']"]
    TOK2["tokenize()\n[HEADER:'TRIG:A:LEVEL', QUERY]"]

    NORM1["normalize_header()\n'CH1:SCALE'"]
    NORM2["normalize_header()\n'TRIGGER:A:LEVEL'"]

    SC1["ScpiCommand\npath='CH1:SCALE'\nis_query=False\nparams=['0.5']"]
    SC2["ScpiCommand\npath='TRIGGER:A:LEVEL'\nis_query=True\nparams=[]"]

    DISPATCH["BaseDevice._dispatch()\nlookup handler in _handlers dict"]

    H1["_ch_scale(cmd)\nsets channels[1].scale = 0.5\nreturns ''"]
    H2["_trig_level(cmd)\nreturns '1.50000000E+00'"]

    RESP["Joined response\n'' + ';' + '1.50000000E+00'\n→ '1.50000000E+00'"]

    RAW --> SPLIT
    SPLIT --> CMD1
    SPLIT --> CMD2
    CMD1 --> TOK1 --> NORM1 --> SC1
    CMD2 --> TOK2 --> NORM2 --> SC2
    SC1 --> DISPATCH
    SC2 --> DISPATCH
    DISPATCH --> H1
    DISPATCH --> H2
    H1 --> RESP
    H2 --> RESP
```

---

## 6. GUI Architecture

```mermaid
graph TB
    subgraph APP["silmulator.app.main()"]
        QA["QApplication"]
    end

    subgraph MW["MainWindow (QMainWindow)"]
        MENU["Menu Bar\nFile / Edit / View / Help"]
        DOCK_L["Left Dock\nBlockLibraryPanel\n(QListWidget)"]
        CENTRAL["Central Widget\nCanvasView"]
        DOCK_B["Bottom Dock\nConsolePanel\n(QPlainTextEdit + QLineEdit)"]
    end

    subgraph CANVAS["Canvas"]
        SCENE["CanvasScene\n(QGraphicsScene)"]
        VIEW["CanvasView\n(QGraphicsView)\nzoom / pan / select"]
        REGISTRY["BLOCK_REGISTRY\ndict[str, type[BaseBlock]]"]
    end

    subgraph BLOCKS["Blocks (QGraphicsRectItem)"]
        B1["OscilloscopeBlock"]
        B2["PowerSupplyBlock"]
        B3["FunctionGeneratorBlock"]
        B4["MultimeterBlock"]
        B5["MSP430Block"]
        B6["CodeInputBlock"]
    end

    subgraph PORTS["Ports & Wires"]
        PORT["ConnectionPort\n(QGraphicsEllipseItem)"]
        WIRE["WireItem\n(QGraphicsPathItem)\nBézier curve"]
    end

    subgraph BUS["Signal Bus (SignalBus)"]
        SIG_CMD["device_command_sent"]
        SIG_RESP["device_response"]
        SIG_CONN["block_connected"]
        SIG_SEL["block_selected"]
        SIG_WAVE["waveform_updated"]
    end

    APP --> MW
    MW --> DOCK_L
    MW --> CENTRAL
    MW --> DOCK_B
    CENTRAL --> VIEW
    VIEW --> SCENE
    SCENE --> BLOCKS
    SCENE --> PORTS
    SCENE --> WIRE
    REGISTRY --> SCENE
    BLOCKS --> BUS
    DOCK_B --> BUS
```

---

## 7. Docker Deployment

```mermaid
graph LR
    subgraph HOST["Host Machine"]
        BROWSER["Web Browser\nlocalhost:6080/vnc.html"]
        DOCKER["Docker Engine"]
    end

    subgraph CONTAINER["Docker Container"]
        XVFB["Xvfb\n:99 virtual display"]
        FLUX["Fluxbox\nwindow manager"]
        APP["Silmulator\n(PyQt6 app)"]
        VNC["x11vnc\n:5900"]
        NOVNC["NoVNC\n:6080"]
    end

    BROWSER -->|"HTTP WebSocket"| NOVNC
    NOVNC -->|"VNC protocol"| VNC
    VNC -->|"X11 frames"| XVFB
    XVFB --> FLUX
    FLUX --> APP
    APP -->|"renders to X11"| XVFB

    DOCKER --> CONTAINER
```

---

## 8. Waveform Signal Chain

```mermaid
flowchart LR
    FGEN["FunctionGenerator\nprocess('SOURCE1:FUNC SIN')\nprocess('SOURCE1:FREQ 1000')\nprocess('SOURCE1:VOLT 2.0')"]

    WUTIL["generate_waveform()\nWaveformType.SINE\nf=1kHz, A=2V"]

    ARR["numpy.ndarray\n10000 samples\n@ 10 MSa/s"]

    OSC_INJ["Oscilloscope\n.inject_waveform('CH1', array)"]

    OSC_PROC["Oscilloscope\nprocess('CURVE?')"]

    RESP["ASCII waveform data\n'0.0,0.627,1.176,...'"]

    MEAS["Measurement\nprocess('MEAS1:VALUE?')\n→ '1000.0' Hz"]

    FGEN --> WUTIL
    WUTIL --> ARR
    ARR --> OSC_INJ
    OSC_INJ --> OSC_PROC
    OSC_PROC --> RESP
    OSC_INJ --> MEAS
```

---

*Silmulator v1.0.0 — architecture diagrams*

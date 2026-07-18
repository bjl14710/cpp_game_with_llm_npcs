# Silmulator — Glossary

Terms the learner has demonstrably grasped are added here.

| Term | Definition |
|------|------------|
| ConnectionPort | A `QGraphicsEllipseItem` child of a block that holds `port_id`, `parent_block`, and `connected_to` (the peer port, or None). Coloured blue when free, green when connected. |
| WireItem | A `QGraphicsLineItem` in the scene (not a child of any block) that draws a gold line between two ConnectionPorts. ZValue = -1 so blocks render above it. |
| x_frac / y_frac | Fractional coordinates (0.0–1.0) passed to `add_port()` that are multiplied by the block's BLOCK_WIDTH / BLOCK_HEIGHT to position a port in local coordinates. |
| hardware_docs/ | The folder in the repo where vendor PDFs, SCPI command tables, and notes are stored. Not auto-parsed; feeds the custom-block authoring workflow. |
| custom block spec | A JSON file in `silmulator/custom_blocks/` that declares a block's name, colour, port layout, and SCPI command map. Loaded at startup by `discover_specs()`. |
| SIL | Software-in-the-Loop: both the controller logic and the instruments are simulated in software. No real hardware. |
| HIL | Hardware-in-the-Loop: the real embedded controller runs in real time; only the instruments/plant are simulated. |
| CIL | Code-in-the-Loop: actual source files are the artifact under test; SCPI calls extracted from them are replayed against simulated instruments. The Code Input block enables this. |
| SCPI | Standard Commands for Programmable Instruments (IEEE 488.2). A tree-structured text command language used by lab instruments and simulated here. |

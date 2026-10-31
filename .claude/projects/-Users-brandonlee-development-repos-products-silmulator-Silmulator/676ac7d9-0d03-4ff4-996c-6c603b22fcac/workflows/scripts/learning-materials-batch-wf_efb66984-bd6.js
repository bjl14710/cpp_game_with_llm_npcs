export const meta = {
  name: 'learning-materials-batch',
  description: 'Generate lesson/drill/vocab HTML for issues #20-#23 in parallel',
  phases: [{ title: 'Generate' }],
}

const ROOT = '/Users/brandonlee/development/repos/products/silmulator/Silmulator/Silmulator'

const ITEMS = [
  {
    num: '0011', issue: '20', slug: 'clock-trees-source-aware-frequency',
    concept: 'Clock trees: why reported frequency must follow the selected source',
    commit: '701853d',
    ideas: 'A clock signal is source x divider, not a stored number. The FR5994 clock tree: MCLK/SMCLK select from {LFXT, VLO, LFMODCLK, DCO, MODCLK, HFXT}, ACLK from {LFXT, VLO, LFMODCLK} (CSCTL2 SELM/SELS/SELA encodings); dividers /1../32 (CSCTL3). Fixed-rate sources: VLO ~9.4 kHz, MODCLK ~4.8 MHz typical, LFMODCLK = MODCLK/128 = 37.5 kHz. The bug class: FREQUENCY? that always reads DCO regardless of the selected source silently lies to everything downstream (timer intervals, UART baud). Derived-value queries must walk the tree: resolve source -> base frequency -> apply divider. Worked examples: LFMODCLK/8 = 4688 Hz (integer truncation), why REFO is rejected (no SELM encoding on FR5994).',
    terms: 'clock tree, clock source multiplexer (SELM/SELS/SELA), clock divider (DIVM/DIVS/DIVA), VLO, MODCLK, LFMODCLK, DCO, derived-value query',
  },
  {
    num: '0012', issue: '21', slug: 'pxren-pull-resistor-semantics',
    concept: 'PxREN semantics: one enable bit whose direction lives in another register',
    commit: '4cb71a1',
    ideas: 'Hardware often encodes one logical setting across two registers: PxREN enables the pull resistor, PxOUT (repurposed when the pin is an input) selects pull-up vs pull-down (SLAU367P Table 12-1). Modeling them as independent pullup/pulldown masks creates inexpressible states (both pulls active) that void SIL results. The fix: model the doc registers (resistor_enable + output), derive pullup/pulldown as read-only views (REN & OUT / REN & ~OUT), keep old commands as deprecated aliases that compose the real registers. Pull-aware input reads: undriven input pin with REN set reads the OUT bit. Pin-index validation 0-7 (-222). Register repurposing as a general MCU pattern.',
    terms: 'PxREN, register repurposing, inexpressible state, derived view, deprecated alias, pull-up/pull-down, undriven pin, single source of truth (registers)',
  },
  {
    num: '0013', issue: '22', slug: 'msp430x-20-bit-registers',
    concept: 'MSP430X: 20-bit registers on a 16-bit-looking ISA',
    commit: '07f1f69',
    ideas: 'The MSP430X CPU extends a 16-bit ISA to 20-bit registers/addresses (1 MB space) while staying binary-compatible: R0-R15 are physically 20 bits (SLAU367P 4.3); 16-bit writes clear the upper 4 bits; word addresses must be even. Simulator implications: masking to 0xFFFF silently truncates valid 20-bit values; PC validation (even, <= 0xFFFFF); reset does not jump to a constant - hardware fetches PC from the reset vector at 0xFFFE (default target when unset). Worked examples: 0xFFFFF round-trips, odd PC rejected -222, reset-vector fetch as little-endian word read. Contrast von-Neumann reset flows across MCU families.',
    terms: 'MSP430X/CPUX, 20-bit register, address space, word alignment, reset vector, PUC-time PC fetch, write masking width, binary compatibility',
  },
  {
    num: '0014', issue: '23', slug: 'password-registers-and-puc',
    concept: 'Password-protected registers and PUC semantics on MSP430',
    commit: 'fa7c595 5e2ba08 615b4e1',
    ideas: 'Safety-critical registers guard against runaway code: WDTCTL requires the 0x5A password in the upper byte of every write (SLAU367P 24.2); ANY wrong-password write triggers an immediate PUC (power-up clear) - a reset, not an error code. This is protection BY reset: a corrupted program counter spraying writes cannot silently disable the watchdog. Simulator modeling: wrong password calls the shared reset path (PC to reset vector, peripherals reset); correct password updates WDTHOLD. Related trust-boundary ideas from the same issue: I2C transfers NACK unknown addresses instead of always ACKing (per-address buffer routing, 10-bit UCSLA10 addressing); interrupt state is real but minimal (GIE gate, Table 9-4 vector priorities, no asynchronous firing since time does not pass).',
    terms: 'WDTPW password, PUC (power-up clear), BOR/POR/PUC hierarchy, watchdog, protection-by-reset, NACK, GIE, interrupt vector priority',
  },
]

const SCHEMA = {
  type: 'object',
  required: ['files_written', 'glossary_rows', 'progress_line', 'js_check', 'status'],
  properties: {
    files_written: { type: 'array', items: { type: 'string' } },
    glossary_rows: { type: 'string', description: 'Markdown table rows to append to GLOSSARY.md, exactly matching its existing format' },
    progress_line: { type: 'string', description: 'One line describing this issue for PROGRESS.md session entry' },
    js_check: { type: 'string', description: 'node --check result for the drill inline JS' },
    status: { type: 'string', enum: ['complete', 'partial', 'failed'] },
  },
}

phase('Generate')
log('Generating learning materials 0011-0014 in parallel')

const results = await parallel(ITEMS.map(it => () => agent(
  `Generate learning materials for the Silmulator project. Project root: ${ROOT}. The working tree is on branch feature/issue-23-i2c-wdt-interrupts. Do NOT run any git commands, do NOT edit docs/learning/GLOSSARY.md or PROGRESS.md, do NOT touch any files except the three you create. Other agents are writing sibling files concurrently.

CONCEPT (issue #${it.issue}): "${it.concept}" — ground every example in the real diff: git -C ${ROOT}/.. show ${it.commit.split(' ').join(' && git -C ' + ROOT + '/.. show ')} (read what you need of it).

Core ideas to teach: ${it.ideas}

Create exactly three files, mirroring the structure/CSS/interactive JS of the existing docs/learning/lessons/0009-*.html, drills/0009-*.html, vocab/0009-*.html (read them first):
1. ${ROOT}/docs/learning/lessons/${it.num}-${it.slug}.html — thorough lesson: concept sections with real code excerpts from the commit(s), worked examples with arithmetic verified against the actual code, misconceptions table, 5-question interactive quiz, reveal-answer scenarios.
2. ${ROOT}/docs/learning/drills/${it.num}-${it.slug}.html — 20+ questions, same interactive pattern as drill 0009 (channels, shuffle, MC + written recall with keyword check, self-rating, results screen). Verify inline JS with: node --check (extract the script body to a temp file under /tmp first).
3. ${ROOT}/docs/learning/vocab/${it.num}-${it.slug}.html — term cards for: ${it.terms}. Definition + project example citing actual code + usage sentence + contrast term each.

Connection resilience: write each file INCREMENTALLY — Write head/CSS + first sections, then append with successive Edits (3-4 sections per call, anchor near the closing </body>). Keep each call under ~200 lines. On any error, check disk state and repeat only the missing step.

Return (structured output): files_written (absolute paths), glossary_rows (markdown rows for these terms, matching GLOSSARY.md's existing table format exactly — read it for the format but do not edit it), progress_line (one line: "issue #${it.issue} — ${it.concept} — lesson/drill/vocab ${it.num}"), js_check (node --check output or "clean"), status.`,
  { label: `learn:${it.num}`, phase: 'Generate', schema: SCHEMA },
)))

const ok = results.filter(Boolean)
log(`${ok.length}/4 generation agents returned`)
return { results: ok.map((r, i) => ({ item: ITEMS[i] ? ITEMS[i].num : '?', ...r })) }
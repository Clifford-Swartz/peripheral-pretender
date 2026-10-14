# Playbooks: adding a board to Peripheral Pretender

Peripheral Pretender should simulate every Microchip development board. Adding one must be a
process that an agent can run, not something only the person who did the last board knows how to
do. These playbooks are that process. They follow the approach of
[zephyr-bsp-copilot](https://github.com/Clifford-Swartz/zephyr-bsp-copilot): Claude works through
fixed phases, scripts do the mechanical parts, Claude makes the judgement calls (schematic
reading, datasheet errata, model behaviour), and every phase ends with a gate that must pass
before the next one starts.

## How to run it

Open a Claude Code session in this repository and say:

> Add the board `<board name>` (`<Microchip part number of the kit>`) to Peripheral Pretender.
> Follow `playbooks/onboard-board.md`.

The agent works through the phases and reports at each gate. Run it in a fresh session (for
example a second VS Code window) with no other context. Wherever the agent gets stuck, asks or
guesses, the playbook is missing something, so fix the playbook, not just the board.

## Files

| File | What it is |
|---|---|
| [onboard-board.md](onboard-board.md) | The procedure: phases, steps, gates |
| [oracles.md](oracles.md) | What to believe when sources disagree: silicon, simulator, datasheet, ATDF |
| [peripherals/](peripherals/) | Lessons per peripheral model, learned against silicon |
| [click-boards.md](click-boards.md) | Adding a Click board or an adapter |

## Phases at a glance

| Phase | Output | Gate |
|---|---|---|
| 0. Inputs | Kit part number, chip, design files, packs, examples located | Every input found or listed as missing |
| 1. Chip | Generated device (registers, IRQs, signals, events) | `py -m pytest tests` green; QEMU builds; `qom-test` passes |
| 2. Board | Board render, board file, QEMU machine | The machine boots `hello` and prints `PP:EXIT 0` |
| 3. Acceptance | Microchip examples run; silicon comparison if the kit is on the desk | Results table; every failure triaged |
| 4. Web | The board in `tools/pp_web` | LEDs and buttons work on the page (web tests) |
| 5. Hand-back | Docs, patches, commit | Full test suite green; commit pushed |

## What the agent does and what the owner does

The agent does everything in the phases, including programming the boards on the desk. The owner
has granted that for the project's development boards. The owner decides which board comes next,
supplies hardware, and reviews the results table. Nothing here is ever sent to upstream QEMU (see
`CLAUDE.md`).

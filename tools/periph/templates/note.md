# $MODULE ($IP) on the $DEVICE: research notes

Gathered for the twin. Sources, most authoritative first: silicon (section 5), the data sheet
(chapter and section numbers), the errata, the ATDF and DFP headers, Harmony, the twin's code.

## 1. Instance facts

| | |
|---|---|
| Base | $BASE |
| NVIC line | $IRQ |
| GCLK channel | $GCLK |
| Events | $EVENTS |
| Pads | $SIGNALS |

## 2. Register map

| Offset | Register | Bits |
|---|---|---|
$REGS

## 3. Behaviour (data sheet)

## 4. Silicon test plan (firmware/hello/tests/${name}probe.c)

| # | Experiment | Prints | Data sheet prediction |
|---|---|---|---|
| E0 | Reset values, writable masks | every register | |

## 5. Answers from silicon

## 6. Open questions

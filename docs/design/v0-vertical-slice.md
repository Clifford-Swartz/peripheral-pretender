# Peripheral Pretender — v0 vertical slice

Status: active. Owner of decisions: the user (Microchip). This file is the shared contract for
everyone (human or agent) building v0. If something here is wrong, record the deviation in your
report; do not silently diverge.

> Paths and commands in §2 describe the original Windows workstation (MSYS2 UCRT64, `C:\Users\...`).
> On any other machine, follow `docs/handoff.md` and `qemu/README.md`: the fork is recreated from
> `qemu/patches/` at `../pp-qemu`, and packs come from `scripts/fetch_packs.py` into `~/.mchp_packs`.

## 0. Goal

Prove every layer once, end to end, on **PIC32CM5164JH01100** as mounted on the
**PIC32CM JH01 Curiosity Pro (EV81X90A)**:

```
ATDF (DFP 1.3.221) ──► generator + per-IP behavior overlays ──► generated register tables
                                                                    │
QEMU fork (pp-qemu) ◄───────────────────────────────────────────────┘
  machine "pic32cm-jh01-cpro" = board ─► SoC ─► armv7m(cortex-m0) + peripheral models
        │ SERCOM4 USART ─► chardev ─► UART text
        ▼
tools: `pp run` CLI and MCP tool `twin_run` ─► {exit code, UART transcript, unimplemented-access summary}
harness: the *same* ELF on the real board (EDBG VCOM) for differential comparison (design + read-only probing in v0)
```

Done means: an unmodified bare-metal ELF built with arm-none-eabi-gcc from the DFP startup files
boots on the twin, brings clocks up exactly the way MCC/Harmony does, prints over SERCOM4, blinks
LED0, and finishes with `PP:EXIT 0`; `pp run` and the MCP tool report that; qtests cover the
register-level contract.

## 1. Fixed decisions (from the user, 2026-10-09)

- Customers: professional embedded teams. Delivery: VS Code extension (alongside MPLAB VS Code
  extensions and Microchip's AI coding assistant) **plus** CI (CLI/container) **plus** MCP. The engine
  must be usable headless; nothing essential lives only in an IDE.
- Build in-house with heavy AI assistance. Upstream QEMU declines AI-generated contributions
  (`pp-qemu/docs/devel/code-provenance.rst:288-349`, repeated in `pp-qemu/AGENTS.md`), so this is a
  **Microchip-maintained downstream fork**. Never prepare or submit any of this code upstream.
- No admin installs on this workstation. User-level installs (`py -m pip install --user`, MSYS2 pacman
  into the existing C:\msys64) are fine.
- Do not `git commit` in either repo; the user commits.

## 2. Workspace and environment

| item | location / command |
|---|---|
| Project repo (this repo) | `C:\Users\C19257\Desktop\Code\Peripheral Pretender` — generator, overlays, devices, firmware, tools, harness, docs |
| QEMU fork | `C:\Users\C19257\Desktop\Code\pp-qemu`, branch `pp/main`, base tag v11.1.2. Path must not contain spaces (`configure:166`). |
| MSYS2 shell | UCRT64 env. From PowerShell: `$env:MSYSTEM='UCRT64'; $env:CHERE_INVOKING='1'; & C:\msys64\usr\bin\bash.exe -l /c/path/to/script.sh`. Write multi-command steps into a `.sh` file first: inline `bash -lc "..."` strings get `$` mangled by PowerShell, and launching MSYS2 bash from Git Bash fails silently. |
| QEMU build | `pp-qemu/build` already configured with `--target-list=arm-softmmu --without-default-features --enable-fdt=system --disable-docs --disable-werror`. Rebuild: `ninja -C /c/Users/C19257/Desktop/Code/pp-qemu/build qemu-system-arm.exe` (meson regenerates automatically when meson.build files change). Full build ≈5 min on 16 cores; incremental is fast. qtests: `ninja -C .../build tests/qtest/<name>.exe` then run with `QTEST_QEMU_BINARY=./qemu-system-arm.exe`. |
| Local patch already in fork | `scripts/symlink-install-tree.py` skips symlink failures on Windows (no Developer Mode). Keep it. |
| Firmware toolchain | `arm-none-eabi-gcc` 16.1 + newlib (UCRT64), `gdb-multiarch` 18.1 (no arm-none-eabi-gdb) |
| Python | Windows `py` = Python 3.14 (PyYAML 6.0.2, mcp, pytest installed; pyserial/pyocd installable with `py -m pip install --user`). The `python` command is a Store stub — never use it. |
| Device pack | `C:\Users\C19257\.mchp_packs\Microchip\PIC32CM-JH_DFP\1.3.221` — ATDF `PIC32CM-JH01/atdf/PIC32CM5164JH01100.atdf`, headers `PIC32CM-JH01/include/`, GCC startup `PIC32CM-JH01/gcc/gcc/startup_pic32cm5164jh01100.c`, linker `PIC32CM-JH01/gcc/gcc/pic32cm5164jh01100_flash.ld`, system file `PIC32CM-JH01/gcc/system_pic32cm5164jh01100.c`. CMSIS core: `C:\Users\C19257\.mchp_packs\ARM\CMSIS\6.1.0\CMSIS\Core\Include`. Read-only. |
| Research (cited, verified 2026-10-09) | `docs/research/*.md`. Essential: `boot-path.md` (every register touched before the first UART byte, every Harmony busy-wait, reset-state traps), `microchip-data.md` (ATDF facts, overlay vocabulary, register tables, NVM rows, IP reuse), `qemu-internals.md` (QEMU 11.1 idioms + cheat sheet), `qemu-board-composition.md` (board-facing hooks), `validation-build.md` (silicon validation + pyOCD/EDBG facts). |
| Prototype generator | `generator/prototype/atdf2qemu.py` + `generator/prototype/overlays/sercom_u2201.yaml` (from research; works for SERCOM USART_INT). |

QEMU 11.1 idioms to use (see `docs/research/qemu-internals.md` cheat sheet): `OBJECT_DECLARE_SIMPLE_TYPE`,
`DEFINE_TYPES`, `class_init(ObjectClass *, const void *)`, `static const Property x[]` without
END_OF_LIST, three-phase Resettable reset (`rc->phases.hold`), `DEVICE_LITTLE_ENDIAN`, `CharFrontend`
type name, headers under `include/hw/core/` and `include/system/`. No `//` comments in pp-qemu C files
(QEMU style: `/* */`, 4-space indent, 80 columns).

## 3. Provenance and licensing rules

- Every new C/H file in pp-qemu starts with:
  ```c
  /*
   * <one-line description>
   *
   * Peripheral Pretender: a personal project. AI-assisted code;
   * not for submission to upstream QEMU.
   *
   * SPDX-License-Identifier: GPL-2.0-or-later
   */
  ```
- Generated files add: `GENERATED by generator/atdf2qemu.py from <ATDF file name> (PIC32CM-JH_DFP <version>). Do not edit.`
- Project-repo source files carry `Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).`
  Open legal questions to keep visible (do not resolve them in code): ATDF data is Apache-2.0 while the
  QEMU fork is GPLv2; GPL obligations when shipping binaries; AI-generated code.

## 4. Architecture of v0

### 4.1 QEMU side (pp-qemu)

1. **`mchp_regfile` helper** (`include/hw/misc/mchp_regfile.h`, `hw/misc/mchp_regfile.c`): byte-lane
   dispatcher over generated `MchpRegInfo` tables (contract in §5.1). QEMU's own register API cannot be
   used directly: it matches exact register start addresses and assumes one width per block, while SAM
   peripherals mix 8/16/32-bit registers at byte offsets and accept accesses spanning them.
   This helper is the single piece of QEMU-facing glue every model uses; keep it small and clean.
2. **`mchp-regblock` generic device** (`hw/misc/mchp_regblock.c`): a SysBusDevice = regfile + one
   generated table + heuristic default behavior + an IRQ output = (INTFLAG & INTENSET) when those
   registers exist. Every ATDF peripheral instance without a dedicated model is instantiated as a
   regblock, so firmware sees correct reset values, store-and-read-back, W1C flags, self-clearing SWRST
   and SYNCBUSY reading 0, with `LOG_UNIMP` naming the instance+register when software writes something
   whose behavior is not modeled. Unmapped holes on HPB0–3 get `create_unimplemented_device` backgrounds.
3. **Dedicated behavior in v0** (built on the regfile; hooks keyed per IP):
   - Clock system: OSCCTRL, OSC32KCTRL, GCLK, MCLK — ready/lock bits derived from enables
     (OSC48MRDY=1 while OSC48MCTRL.ENABLE, XOSCRDY after XOSCCTRL.ENABLE if the board declares a
     crystal, XOSC32KRDY/OSC32KRDY likewise, DPLLSTATUS LOCK|CLKRDY once enabled with a valid
     reference), all SYNCBUSY = 0, GCLK GENCTRL0 reset 0x00000106, PCHCTRL CHEN reads back,
     MCLK.INTFLAG.CKRDY = 1 (set again after every CPUDIV write), GCLK SWRST self-clears.
     **Frequency propagation**: compute OSC48M (48 MHz/(OSC48MDIV+1)), XOSC (board property, 32 MHz on
     the Curiosity Pro), XOSC32K/OSC32K/OSCULP32K (32768 Hz), FDPLL96M (per datasheet ratio formula),
     GCLK generators (SRC, DIV, DIVSEL, GENEN), CPU = GCLK0 / MCLK.CPUDIV, and drive the armv7m
     `cpuclk` Clock (4 MHz at reset, 48 MHz after Harmony init). Expose per-channel peripheral Clocks
     (PCHCTRL m) as Clock outputs so SERCOM etc. can read their core clock.
   - NVMCTRL (03929): INTFLAG.READY=1 when idle (re-asserted after W1C), PARAM = 0x08032000
     (read-only), CTRLB stored (reset 0x00000080), CTRLA command key 0xA5 accepted and completed
     instantly for no-op commands; flash programming is out of scope (log LOG_UNIMP).
   - SUPC: STATUS/INTFLAG BVDDSRDY=1 and BODVDDRDY=1 when BODVDD.ENABLE; BODVDD reset loaded from the
     user row (§4.3).
   - RSTC: RCAUSE = 0x01 (POR) on cold start, 0x40 (SYST) after SYSRESETREQ, 0x20 after a WDT reset
     (WDT timeout itself is out of scope). RCAUSE must survive the system reset it describes; wire
     armv7m's SYSRESETREQ output so the SoC records SYST before requesting the reset.
   - WDT: CTRLA/CONFIG/EWCTRL reset values loaded from the user row (production row → disabled,
     PER/WINDOW/EWOFFSET 0xB); SYNCBUSY reads 0; CLEAR accepts 0xA5. No timeout in v0 (LOG_UNIMP if
     enabled).
   - PAC: STATUSB reset 0x00000002, STATUSC 0x09000000, WRCTRL bookkeeping (no enforcement in v0).
   - DSU: DID = 0x11060100 (rev B0) by default with a SoC property `silicon-rev` ("B0"/"C0" → 0x1/0x2
     in DID[11:8]); DSU_EXT mirror at +0x100; CoreSight ROM table IDs from ATDF.
   - DIVAS: compute DIV/SQRT instantly (BUSY=0, DBZ on divide by zero), plus IOBUS alias at 0x60000200.
   - PM: SLEEPCFG/STDBYCFG store and read back.
   - SERCOM (all 8 instances, dedicated device `mchp-sercom`): USART_INT mode fully per §5 of
     `boot-path.md` finding 18 and the overlay; register view chosen by CTRLA.MODE; other modes
     store/read-back with LOG_UNIMP. Creates (but v0 does not drive) an I2C bus named
     `sercom<N>.i2c` and an SSI bus named `sercom<N>.spi` so board parts can attach later.
   - PORT (`mchp-port`): 3 groups at 0x80 stride; DIR/OUT + SET/CLR/TGL aliases, IN computed from
     DIR/OUT/PULLEN/INEN and external input levels, PINCFG/PMUX byte registers, WRCONFIG, IOBUS alias
     at 0x60000000; group-0 PMUX15 reset 0x06 (§5.3 reset overrides).
4. **SoC** `pic32cm5164jh01100-soc` (`hw/arm/pic32cm_jh_soc.c`, `include/hw/arm/pic32cm_jh_soc.h`):
   armv7m with cpu-type cortex-m0, num-irq 32, num-prio-bits 2, mpu-ns-regions 8 (verify MPU_TYPE
   reads 8 regions; if it does not work, document it), init-svtor 0; flash 512 KB at 0x0 (ROM;
   rom_device later), data flash 8 KB at 0x00400000, NVM rows ROM 0x00800000–0x0080FFFF (§4.3), SRAM
   64 KB at 0x20000000, IOBUS aliases, every instance from the generated instance table, OR gates for
   shared vectors (§5.5). Try to make SCB->CPUID read the M0+ value 0x410CC601 (e.g. a machine/global
   property on the CPU's `midr` set before armv7m realize); if not feasible in v0, document it.
5. **Board** `pic32cm-jh01-cpro` (`hw/arm/pic32cm_jh01_cpro.c`): XOSC 32 MHz and XOSC32K 32.768 kHz
   present; SoC; LED0 = `TYPE_LED` on PC05, active-low; SW0 on PB19 as a board QOM property
   `sw0-pressed` (bool, runtime-settable via qom-set; pressed drives PB19 low, released = pulled up);
   serial mapping `serial_hd(0)` → SERCOM4 (EDBG VCOM), `serial_hd(1)` → SERCOM2 (EXT1),
   `serial_hd(2)` → SERCOM3 (EXT2), `serial_hd(3)` → SERCOM6 (EXT3), `serial_hd(4)` → SERCOM0 (LIN);
   `armv7m_load_kernel(..., 0, 512 KiB)`; `docs/system/arm/pic32cm-jh01-cpro.rst`.
6. Build integration: Kconfig (`PIC32CM_JH_SOC`, `PIC32CM_JH01_CPRO`, `MCHP_REGFILE`, `MCHP_SERCOM`,
   `MCHP_PORT`), meson source sets, trace-events for SERCOM TX/RX, PORT pin changes, clock frequency
   changes, regblock unimplemented writes.

### 4.2 Project side

- `generator/` (`atdf2qemu.py` + modules), `overlays/<ip>_<id>.yaml`, `devices/pic32cm5164jh01100.json`.
- `firmware/hello/` — bare-metal test image (§6.2).
- `tools/pp/` — CLI runner; `tools/pp_mcp/` — MCP server; `tests/` — pytest suites.
- `harness/` + `docs/silicon-harness.md` — silicon comparison design and scripts.

### 4.3 NVM rows (0x00800000–0x0080FFFF, read-only ROM, default fill 0xFF)

| address | content (default image) |
|---|---|
| 0x00800000 CAL (8 B) | 0xFF (contents undocumented) |
| 0x00802000 LOCKBIT | 0xFF… (no security bit) |
| 0x00804000 USER row (256 B) | words 0/1 = **0xB15088FB / 0xFFFFF8BB** (production values, DS60001632H Table 10-3); rest 0xFF. Do **not** use the DFP default 0xFFFFFFFF (it would mean an always-on WDT and BODVDD off). |
| 0x00806000 OTP1 | NVM geometry consistent with NVMCTRL.PARAM 0x08032000 |
| 0x00806020 OTP5 / software calibration row (64-bit, read with a 64-bit load by Harmony) | plausible values: ADC bias fields mid-scale, OSC32K CAL [18:12] mid-scale, CAL48M [40:19] = 0x00200220-compatible; document that real values must be dumped from silicon |
| 0x00806030 TEMP_LOG | 0xFF |
| 0x0080A00C, 0x0080A040/44/48 serial number | deterministic default (e.g. 0x50500000, 0x00000001, 0x00000002, 0x00000003), overridable by SoC property `serial-number` (32 hex digits) |

Images that contain fuse words at 0x00804000/04 (XC32 `#pragma config`) must be honored: the ELF
loader writes into this ROM region; reset logic (WDT/SUPC/NVMCTRL.LOCK) reads the row at reset.

## 5. Contracts

### 5.1 `MchpRegInfo` and the regfile API (`include/hw/misc/mchp_regfile.h`) — copy verbatim

```c
typedef struct MchpRegInfo {
    const char *name;   /* ATDF register name, e.g. "CTRLA", "PCHCTRL" */
    uint16_t offset;    /* byte offset of element 0 inside the register group */
    uint8_t size;       /* 1, 2 or 4 bytes */
    uint8_t count;      /* array elements (1 for scalars) */
    uint16_t stride;    /* bytes between array elements (== size unless ATDF says otherwise) */
    int16_t alias_of;   /* index in the same table of the storage register this one aliases,
                           or -1; may equal its own index (e.g. INTENSET is the storage) */
    uint32_t reset;     /* reset value of every element (exceptions: MchpResetOverride) */
    uint32_t impl;      /* implemented bits; reads return value & impl (0xFFFFFFFF if ATDF has no bitfields) */
    uint32_t wmask;     /* software-writable bits (RW bits minus ro/w1c) */
    uint32_t w1c;       /* write-1-to-clear bits */
    uint32_t rdz;       /* bits that always read 0 (strobes, self-clearing, write-only) */
    uint32_t enprot;    /* enable-protected bits: writable only while the IP is disabled */
    uint32_t syncbusy;  /* SYNCBUSY bits raised by writing this register (0 = none) */
    uint32_t flags;     /* MCHP_F_* */
} MchpRegInfo;

#define MCHP_F_PAC             (1u << 0)  /* PAC write-protectable */
#define MCHP_F_ALIAS_SET       (1u << 1)  /* write 1 sets bits of alias_of; read returns alias_of */
#define MCHP_F_ALIAS_CLEAR     (1u << 2)  /* write 1 clears bits of alias_of; read returns alias_of */
#define MCHP_F_ALIAS_TOGGLE    (1u << 3)  /* write 1 toggles bits of alias_of; read returns alias_of */
#define MCHP_F_DYNAMIC         (1u << 4)  /* value produced by the model's pre_read hook */
#define MCHP_F_HOOK_PRE_READ   (1u << 5)  /* call pre_read (side effects only when !attrs.debug) */
#define MCHP_F_HOOK_POST_WRITE (1u << 6)  /* call post_write after storage is updated */
#define MCHP_F_BUSY_BUSERR     (1u << 7)  /* write while own SYNCBUSY bit set = bus error (pedantic mode) */
#define MCHP_F_WRITE_ONLY      (1u << 8)  /* reads return 0 */
#define MCHP_F_READ_ONLY       (1u << 9)  /* writes ignored (+LOG_GUEST_ERROR) */

typedef struct MchpResetOverride {
    uint16_t offset;    /* byte offset of the element */
    uint8_t size;
    uint32_t value;
} MchpResetOverride;

typedef struct MchpRegFile MchpRegFile;
typedef uint32_t (*MchpRegPreReadFn)(MchpRegFile *rf, const MchpRegInfo *ri,
                                     unsigned idx, uint32_t stored,
                                     bool side_effects);
typedef void (*MchpRegPostWriteFn)(MchpRegFile *rf, const MchpRegInfo *ri,
                                   unsigned idx, uint32_t old_value,
                                   uint32_t written, uint32_t we);
typedef bool (*MchpRegEnabledFn)(MchpRegFile *rf);

struct MchpRegFile {
    const char *name;                 /* instance name for logs, e.g. "sercom4" */
    uint8_t *storage;                 /* little-endian backing store */
    uint32_t size;                    /* bytes of storage (register-group size) */
    const MchpRegInfo *regs;          /* active table (SERCOM switches it on CTRLA.MODE) */
    unsigned nregs;
    const MchpResetOverride *ovr;
    unsigned novr;
    void *opaque;                     /* owning device */
    MchpRegPreReadFn pre_read;        /* may be NULL */
    MchpRegPostWriteFn post_write;    /* may be NULL */
    MchpRegEnabledFn is_enabled;      /* NULL = never enabled (no enable protection) */
};

void mchp_regfile_init(MchpRegFile *rf, const char *name, uint32_t size,
                       const MchpRegInfo *regs, unsigned nregs);
void mchp_regfile_set_table(MchpRegFile *rf, const MchpRegInfo *regs, unsigned nregs);
void mchp_regfile_set_overrides(MchpRegFile *rf, const MchpResetOverride *ovr, unsigned n);
void mchp_regfile_reset(MchpRegFile *rf);       /* reset values, then overrides */
uint64_t mchp_regfile_read(MchpRegFile *rf, hwaddr addr, unsigned size, MemTxAttrs attrs);
void mchp_regfile_write(MchpRegFile *rf, hwaddr addr, uint64_t val, unsigned size, MemTxAttrs attrs);
uint32_t mchp_regfile_peek(MchpRegFile *rf, uint32_t offset, unsigned size);   /* raw storage */
void mchp_regfile_poke(MchpRegFile *rf, uint32_t offset, unsigned size, uint32_t val);
const MchpRegInfo *mchp_regfile_find(MchpRegFile *rf, const char *name);       /* active table */
/* VMState helper: VMSTATE_VBUFFER_UINT32 or equivalent over storage/size */
```

Semantics:
- MMIO ops using the helper: `.valid = {.min_access_size = 1, .max_access_size = 4, .unaligned = false}`,
  `.impl` the same, `DEVICE_LITTLE_ENDIAN`, read/write via `*_with_attrs` so `attrs.debug` is visible.
- **Read** of [addr, addr+size): for every register element overlapping the range, take the stored
  value; if `WRITE_ONLY` → 0; if `DYNAMIC` or `HOOK_PRE_READ` → `pre_read(rf, ri, idx, stored, !attrs.debug)`;
  clear `rdz`; AND with `impl`; place the overlapping bytes. Bytes not covered by any register read 0.
- **Write**: per overlapping element compute the lane mask `we` (bits covered by the access) and the
  lane-aligned value. `READ_ONLY` → ignore + LOG_GUEST_ERROR. Alias registers apply set/clear/toggle
  of `(val & we & ri->wmask)` to the `alias_of` element (same index), then run the alias target's
  post_write hook semantics through the writer's entry (`post_write(rf, ri, ...)` with ri = alias reg).
  Otherwise: `writable = ri->wmask & we`; if `is_enabled(rf)` and `(val ^ old) & ri->enprot & we` →
  LOG_GUEST_ERROR and drop those bits; `new = (old & ~writable) | (val & writable)`;
  `new &= ~(val & we & ri->w1c)`; store; post_write if flagged; finally clear `rdz` bits in storage.
  v0 never returns bus errors (no PAC enforcement, instant sync).
- Unknown/reserved bytes: writes ignored with LOG_GUEST_ERROR; reads 0.

### 5.2 Overlay schema (`overlays/<ip-lowercase>_<id-lowercase>.yaml`)

```yaml
ip: SERCOM
id: U2201
versions: ["3.1.1"]            # generator refuses other versions (warns and falls back to heuristics)
modes:                          # or `registers:` directly when the IP has no modes
  USART_INT:
    registers:
      CTRLA:
        pac_protected: true
        enable_protected: {all_except: [SWRST, ENABLE]}
        sync: {SWRST: SWRST, ENABLE: ENABLE}    # field -> SYNCBUSY bit
        self_clearing: [SWRST]
        hooks: [post_write]
      INTENCLR: {alias: {storage: INTENSET, op: clear}}
      INTENSET: {alias: {storage: INTENSET, op: set}}
      INTFLAG: {w1c: [TXC, RXS, CTSIC, RXBRK, ERROR], ro: [DRE, RXC], irq_source: {enable: INTENSET}}
      STATUS: {w1c: [PERR, FERR, BUFOVF, ISF, COLL, TXE], ro: [CTS]}
      SYNCBUSY: {dynamic: true}
      DATA: {hooks: [pre_read, post_write]}
reset_overrides: {"GENCTRL[0]": 0x00000106}     # per-element exceptions (also PORT group0 PMUX[15]=0x06)
reset_from: {}                                   # documentation of user-row-loaded fields (model code implements)
```

Vocabulary (from `docs/research/microchip-data.md` table "Behavior overlay vocabulary"): `w1c`, `ro`,
`alias`, `enable_protected` (`all` | `{all_except: [...]}`), `sync`, `write_while_syncbusy: bus_error`,
`strobe`, `self_clearing`, `pac_protected`, `hooks` (`pre_read`, `post_write`), `dynamic`,
`reset_overrides`, `reset_from`, `irq_source`, `write_only`, `read_only`.

**Heuristics for IPs without an overlay** (all reported in the generator report): registers named
`SYNCBUSY`/`*SYNCBUSY` → dynamic, reading 0; bitfields named `SWRST` → self_clearing; `CMD` fields in
`CTRLBSET`/`CTRLBCLR` → self_clearing; ATDF `atomic-op="clear:<self>"` on INTFLAG → w1c; ATDF
`set:/clear:/toggle:<other>` → alias; `INTENSET`/`INTENCLR` pairs → alias of INTENSET; ATDF `rw="R"` →
READ_ONLY; `rw="W"` → WRITE_ONLY; missing initval → 0 (reported).

### 5.3 Generated outputs

| file | content |
|---|---|
| `pp-qemu/include/hw/mchp/gen/<ip>_<id>.h` (e.g. `sercom_u2201.h`) | per IP and per mode: `REGn`/`FIELD` constants via `hw/core/registerfields.h` with prefix `MCHP_<IP>[_<MODE>]_`; `static const MchpRegInfo mchp_<ip>[_<mode>]_regs[]`; `#define MCHP_<IP>[_<MODE>]_NREGS`; `#define MCHP_<IP>_SIZE` (register-group bytes); `static const MchpResetOverride mchp_<ip>_reset_overrides[]` where needed. Header guard, includes `hw/misc/mchp_regfile.h`. |
| `pp-qemu/include/hw/arm/pic32cm5164jh01100_gen.h` | memory segments; `MchpInstanceInfo` table (`name`, `module`, `ip_id`, `ip_version`, `base`, `slot_size` (0x400 APB slot unless ATDF says otherwise), `irq` (-1 none), `pac_id`, `gclk_core`, `gclk_slow`, `dmac_rx`, `dmac_tx` (-1 none), `regs`/`nregs` default table); shared-vector membership; NVIC parameters; pad/signal table (`pad`, `function letter`, `instance`, `group`, `index`) for future pad routing. Define the `MchpInstanceInfo` struct in `include/hw/misc/mchp_regfile.h` or a sibling header `include/hw/mchp/mchp_device.h` (generator owns that sibling). |
| `pp-qemu/hw/arm/pic32cm5164jh01100_nvmrows.h` | default NVM-rows image builder per §4.3 (function filling a buffer). |
| `devices/pic32cm5164jh01100.json` (project) | same device description for tools/harness/VS Code (memory map, instances, IRQs, signals, register maps with offsets/sizes/reset/access). |
| `generator/out/report-pic32cm5164jh01100.md` | provenance of every semantic (ATDF / overlay / heuristic), conflicts, missing initvals. |

### 5.4 QOM and naming

- Machine `pic32cm-jh01-cpro`; SoC at `/machine/soc`, type `pic32cm5164jh01100-soc`.
- Peripheral children named by lowercase ATDF instance name: `/machine/soc/sercom4`,
  `/machine/soc/port`, `/machine/soc/gclk`, …
- SERCOM buses `sercom<N>.i2c`, `sercom<N>.spi` (unique names). SERCOM chardev property `chardev`.
- PORT GPIO: named gpio-out `port-out[96]` and gpio-in `port-in[96]`, index = group*32 + pin
  (PA=0, PB=1, PC=2). PORT also provides a runtime QOM property per group, e.g. `pa-in` (uint32,
  qom-settable external input levels) and read-only `pa-out`.
- Board property `sw0-pressed` (bool).

### 5.5 IRQs

num-irq 32, 2 priority bits. OR gates (TYPE_OR_IRQ) for shared vectors: 0 = MCLK|OSCCTRL|OSC32KCTRL|PAC|PM|SUPC,
9 = SERCOM0|SERCOM6, 10 = SERCOM1|SERCOM7, 20 = TC0|TC5, 21 = TC1|TC6, 22 = TC2|TC7. Take membership from
the generated table, not from hand-written lists. SERCOM4 (VCOM) is IRQ 13. Every IRQ output must be
level = (INTFLAG & INTEN) != 0, updated after every state change.

### 5.6 Firmware exit / run protocol

Firmware prints a final line `PP:EXIT <decimal>` (0 = pass). Runners stop QEMU when they see it. Do not
use semihosting in images meant for silicon (BKPT 0xAB faults without a debugger attached).

### 5.7 Logging, tracing, debugger safety

- `LOG_UNIMP` for unmodeled behavior (always include instance and register name), `LOG_GUEST_ERROR` for
  misuse (enable-protected write, invalid width, reserved address).
- No side effects on reads with `attrs.debug` (gdbstub, monitor `x`): e.g. reading SERCOM DATA from GDB
  must not pop the RX buffer.
- Trace events: `mchp_sercom_tx`, `mchp_sercom_rx`, `mchp_port_out`, `mchp_clk_update`,
  `mchp_regblock_unimp_write`.
- QEMU calls `cpu_abort` on lockup (BKPT in a debug-build fault handler). Release builds only in tests.

## 6. Components and acceptance criteria

### 6.1 Generator (`generator/`, `overlays/`, generated files in §5.3)
- Evolve `generator/prototype/atdf2qemu.py` into a maintainable generator (stdlib + PyYAML, `py`).
- Overlays for: SERCOM U2201 (USART_INT complete; SPIM/SPIS/I2CM/I2CS minimal), PORT U2210,
  OSCCTRL U2119, OSC32KCTRL U2246, GCLK U2122, MCLK U2234, NVMCTRL 03929, SUPC U2117, PM U2240,
  RSTC U2239, WDT U2251, PAC U2120, DSU U2209, DIVAS U2258, EVSYS U2256. Everything else: heuristics.
- Version guard + unknown-register/field errors; reproducible output (sorted, stable).
- Acceptance: all generated headers compile with `gcc -std=gnu11 -Wall -Wextra -fsyntax-only` against a
  shim that defines `MchpRegInfo` exactly as §5.1; pytest tests for parser/overlay/merge (W1C/RO,
  aliases, enable-protect masks, reset overrides incl. GENCTRL0 and PORT PMUX15); SERCOM USART table
  equals the research table in `docs/research/microchip-data.md`; report written.

### 6.2 Firmware (`firmware/hello/`)
- Bare metal for PIC32CM5164JH01100: DFP GCC startup + linker script + DFP headers + CMSIS; Makefile
  using arm-none-eabi-gcc (`-mcpu=cortex-m0plus -mthumb -Os -g`), no XC32 dependencies, no semihosting.
- Sequence (print one line per step on SERCOM4 @115200 8N1 — PB10 TX PAD2, PB11 RX PAD3, TXPO=1,
  RXPO=3): replicate Harmony's `SYS_Initialize` register sequence for this board exactly as listed in
  `docs/research/boot-path.md` (NVMCTRL.CTRLB RWS, PORT PINCFG/PMUX for PB10/PB11, PC05 output, the
  64-bit calibration-row read, OSC48MDIV=0 + OSC48MSYNCBUSY wait, OSC48MRDY wait, GENCTRL0 + SYNCBUSY
  wait, PCHCTRL[22] CHEN read-back wait, MCLK APBCMASK, SERCOM4 CTRLA=0x40310104/BAUD=63019/
  CTRLB=0x30000 with SYNCBUSY waits and ENABLE); then print banner, DSU.DID, SCB->CPUID, the 4 serial
  number words, the calibration row, SysTick-timed LED0 blinks (3x, PC05 active-low), and finally
  `PP:EXIT 0`; then idle (WFI loop).
- A blocking polled TX path (DRE) for all prints; one interrupt-driven print using INTENSET.DRE + IRQ13
  ISR to exercise the NVIC path.
- Acceptance: builds warning-free; `arm-none-eabi-size` reported; map file shows vector table at 0;
  runs unchanged on silicon (VCOM) and on the twin.

### 6.3 Tools (`tools/pp/`, `tools/pp_mcp/`, `tests/`)
- `py -m tools.pp run <elf> [--machine pic32cm-jh01-cpro] [--qemu PATH] [--timeout S]
  [--icount SHIFT] [--gdb] [--trace PATTERN] [--input TEXT] [--json] [--extra-args ...]`:
  launches QEMU with the UART on a TCP socket or stdio (pick what works reliably on Windows with
  subprocess pipes — test it), collects the transcript, stops at `PP:EXIT n` and returns n; timeout →
  exit 124 with partial transcript; always passes `-d unimp,guest_errors -D <tmp log>` and returns a
  summary of unimplemented/guest-error accesses grouped by instance/register (the "model backlog").
  Default QEMU path: `C:/Users/C19257/Desktop/Code/pp-qemu/build/qemu-system-arm.exe`.
- MCP server (`py -m tools.pp_mcp`), stdio transport, tools: `twin_run(elf_path, timeout_s=10,
  input_text=None, icount_shift=None)` → `{exit_code, uart, unimplemented_summary, qemu_stderr_tail,
  duration_s}`; `twin_machines()`.
- Tests: pytest unit tests for parsing/summaries; an e2e test that runs `firmware/hello` on the twin
  (skipped with a clear reason if the machine or ELF is missing). Validate the runner against the
  existing `mps2-an385` machine with a tiny CMSDK-UART test image so it is proven before the PIC32CM
  machine exists.

### 6.4 Silicon harness (`harness/`, `docs/silicon-harness.md`)
- Design the differential flow: program (pyOCD + CMSIS pack built from the DFP, or MCC MCP
  `mplab_program_device`), VCOM capture (pyserial, EDBG needs DTR asserted), golden reset-value dump,
  register snapshots at breakpoints via one GDB/MI script against both pyOCD gdbserver and the QEMU
  gdbstub, comparison rules (event order and values, not absolute time; mask volatile registers),
  board-farm CI.
- Implement scripts as far as possible. **Hardware rule: read-only.** You may install pyocd/pyserial
  with `py -m pip install --user`, list probes, and read memory (e.g. DSU.DID at 0x41002018). Do not
  program, erase, reset, or halt the board; the user must approve that first.

### 6.5 Core QEMU work — part A (regfile, regblock, system devices, SoC, board, build)
- Everything in §4.1 except the dedicated SERCOM and PORT devices (use regblocks for them for now).
- Acceptance: `qemu-system-arm -M help` lists `pic32cm-jh01-cpro`; `-M pic32cm-jh01-cpro -kernel
  firmware/hello/build/hello.elf -d unimp,guest_errors` runs without any assertion/abort; with
  `-s -S` + gdb-multiarch (or HMP) show that execution passes the clock bring-up (no hang on OSC48MRDY
  or PCHCTRL CHEN) and that cpuclk is 48 MHz afterwards; `info mtree` shows the full map; a qtest
  `tests/qtest/pic32cm-jh-test.c` checks reset values (GENCTRL0=0x106, NVMCTRL.PARAM, DSU.DID,
  PAC STATUSB/C, user row words, SYNCBUSY=0), ready bits, and RCAUSE=POR.

### 6.6 Core QEMU work — part B (SERCOM, PORT, LED, end to end)
- `mchp-sercom` and `mchp-port` per §4.1, replacing their regblocks in the SoC; LED0 and SW0 on the
  board; trace events.
- Acceptance: `py -m tools.pp run firmware/hello/build/hello.elf` returns 0 with the full transcript;
  `-trace led_set_intensity` (or the PORT trace) shows LED0 toggling; qtests for SERCOM USART
  (reset values, W1C INTFLAG not clearing DRE/RXC, INTENSET/INTENCLR alias, STATUS W1C, enable-protect,
  DATA TX to a socket chardev, RX → RXC → IRQ, debug read does not pop RX) and PORT (SET/CLR/TGL, IN,
  PINCFG/PMUX bytes, IOBUS alias, gpio-out levels); all existing arm qtests that build still pass for
  the machines touched by shared code.

## 7. Out of scope for v0

SERCOM SPI/I2C transfers, DMAC, EIC, TC/TCC, RTC behavior, ADC/DAC/AC, CAN (M_CAN), WDT timeout,
PAC enforcement and pedantic sync timing, flash programming through NVMCTRL, sleep modes, PTC,
cortex-m0plus CPU type, pad-level pin-mux routing, external part protocol, VS Code extension.
Record anything you find that these will need.

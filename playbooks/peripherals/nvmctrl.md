# NVMCTRL (03929): flash programming

The PIC32CM JH/MC flash controller in `hw/misc/mchp_sysctrl.c` (`mchp-nvmctrl`), with the flash
arrays in `hw/arm/pic32cm_soc.c`. Research and silicon answers:
[docs/research/peripherals/nvmctrl-03929.md](../../docs/research/peripherals/nvmctrl-03929.md).
Checked on the MC00 by `firmware/hello` APP=nvmctrl (golden `nvmctrl-silicon.txt`) and the qtest
`pic32cm-nvmctrl-test`.

## How it plugs in

- The SoC maps the main array, the data flash and the NVM rows as ROM devices
  (`memory_region_init_rom_device`): reads and instruction fetches go straight to RAM, writes go
  to `mchp_nvmctrl_load()`, the page buffer. `pic32cm_connect_nvm()` hands the arrays to the
  NVMCTRL (`mchp_nvmctrl_attach()`); other NVMCTRLs (SAM E5x U2409) leave writes ignored.
- The model programs through `address_space_write_rom()`, which invalidates translated code, so
  firmware that rewrites code in flash runs the new code.
- `hw/core/loader.c` treats ROMD regions as ROM: the ELF is written once, at the first reset, and
  what the guest programs survives system resets.

## Lessons

- **ADDR is relative to the array and 16 bits wide** on the MC00, not the data sheet's 22 bits
  of absolute address. A probe that computes ADDR from absolute addresses must check it first
  (`nvmctrlprobe.c` skipped its main array experiments when ADDR did not read as expected).
- **16-bit page buffer writes work, 8-bit writes fault and still load.** The data sheet says
  both fault.
- **Guard every probe that programs flash**: refuse SSB, SCEHL (permanent), EAR and WAP, and any
  main array command outside a scratch row far above the image.
- **Main array operations stall the CPU's flash fetches**: take the start time before the
  command write, and time with SysTick over its full 24 bits (wraps are lost during the stall).
- **The NVM cache and fetch timing change results** (PRM, a command while busy): see
  [../model-peripheral.md](../model-peripheral.md#lessons-from-silicon).

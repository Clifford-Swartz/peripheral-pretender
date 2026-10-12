import sys
BS = chr(92)
def echo(text):
    return "echo " + text.replace(" ", BS + " ") + BS + "n"
regs = [
    ("NVMCTRL.CTRLB (expect 0x84)", "x/wx 0x41004004"),
    ("OSCCTRL.STATUS (OSC48MRDY=0x10)", "x/wx 0x4000100C"),
    ("OSCCTRL.OSC48MDIV (expect 0)", "x/bx 0x40001015"),
    ("OSCCTRL.CAL48M", "x/wx 0x40001038"),
    ("GCLK.GENCTRL0 (expect 0x00010106)", "x/wx 0x40001C20"),
    ("GCLK.SYNCBUSY", "x/wx 0x40001C04"),
    ("GCLK.PCHCTRL22 (expect 0x40)", "x/wx 0x40001CD8"),
    ("MCLK.APBCMASK (expect 0x08000020)", "x/wx 0x4000081C"),
    ("MCLK.CPUDIV", "x/bx 0x40000804"),
    ("SERCOM4.CTRLA", "x/wx 0x42001400"),
    ("SERCOM4.BAUD", "x/hx 0x4200140C"),
    ("SERCOM4.INTFLAG (DRE=0 in part A)", "x/bx 0x42001418"),
    ("SysTick LOAD (47999 = 48 MHz / 1 kHz - 1)", "x/wd 0xE000E014"),
    ("SCB.CPUID", "x/wx 0xE000ED00"),
    ("MPU_TYPE", "x/wx 0xE000ED90"),
    ("DSU.DID", "x/wx 0x41002018"),
    ("NVM user row", "x/2wx 0x00804000"),
    ("calibration row", "x/2wx 0x00806020"),
    ("serial word 0", "x/wx 0x0080A00C"),
    ("serial words 1-3", "x/3wx 0x0080A040"),
]
out = ["set pagination off", "set confirm off", "target remote :1234",
       echo("=== at reset"), "info registers pc sp", "x/i $pc",
       echo("RSTC.RCAUSE:"), "x/bx 0x40000C00",
       echo("GCLK.GENCTRL0:"), "x/wx 0x40001C20",
       echo("OSCCTRL.OSC48MDIV:"), "x/bx 0x40001015",
       "break *0x480", "continue",
       echo("=== at the SERCOM4 DRE poll (console_putc)"),
       "info registers pc", "x/3i $pc", "bt"]
for label, cmd in regs:
    out += [echo(label + ":"), cmd]
out += ["delete", "stepi", "stepi", "stepi", "stepi",
        echo("=== still polling DRE after 4 more instructions"),
        "info registers pc",
        echo("=== info qtree"), "monitor info qtree",
        echo("=== info mtree -f"), "monitor info mtree -f"]
open(sys.argv[1], "w", newline="\n").write("\n".join(out) + "\n")

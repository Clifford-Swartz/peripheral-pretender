"""Correct datasheet section numbers in comments (checked against ds.txt)."""
import os

ROOT = "C:/Users/C19257/Desktop/Code/pp-qemu"
EDITS = {
    "hw/misc/mchp_clock.c": [
        ("(DS 15.8.4)", "(DS 15.6.4)"),
    ],
    "hw/misc/mchp_sysctrl.c": [
        ("reads 1 (DS 18.8.4). */", "reads 1 (DS 18.6.4). */"),
        ("/* RCAUSE must describe the reset it survives (DS 19.6.2). */",
         "/* RCAUSE must describe the reset it survives (DS 20.7.1). */"),
        ("locks CTRLA (DS 22.8.1) */", "locks CTRLA (DS 22.6.1) */"),
        ("at once (DS 22.8.8) */", "at once (DS 22.6.8) */"),
    ],
    "tests/qtest/pic32cm-jh-test.c": [
        ("(DS 14.8.3, ATDF says 0)", "(DS 14.7.3, ATDF says 0)"),
    ],
}

for rel, pairs in EDITS.items():
    path = os.path.join(ROOT, rel)
    with open(path, encoding="utf-8") as f:
        text = f.read()
    for old, new in pairs:
        assert text.count(old) == 1, (rel, old)
        text = text.replace(old, new)
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write(text)
print("ok")

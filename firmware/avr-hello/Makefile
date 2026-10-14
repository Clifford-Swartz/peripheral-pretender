# avr-hello: first-light firmware for the AVR64EA48 Curiosity Nano.
# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
#
# Builds with the avr-gcc in MPLAB XC8 and the AVR-Ex device pack:
#   make -C firmware/avr-hello            (in an MSYS2 UCRT64 shell)
# XC8 and DFP default to the MPLAB X install; override XC8= / DFP=.

XC8    ?= /c/Program Files/Microchip/xc8/v2.50
DFP    ?= /c/Program Files/Microchip/MPLABX/v6.20/packs/Microchip/AVR-Ex_DFP/2.8.189
DEVICE := avr64ea48
CC     := "$(XC8)/avr/bin/avr-gcc"
OBJCOPY:= "$(XC8)/avr/bin/avr-objcopy"
BUILD  := build

CFLAGS := -mmcu=$(DEVICE) -B "$(DFP)/gcc/dev/$(DEVICE)" -isystem "$(DFP)/include" \
          -O2 -std=gnu11 -Wall -Wextra -ffunction-sections -fdata-sections
LDFLAGS:= -Wl,--gc-sections

all: $(BUILD)/hello.elf $(BUILD)/hello.hex

$(BUILD)/hello.elf: main.c Makefile
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ main.c

$(BUILD)/hello.hex: $(BUILD)/hello.elf
	$(OBJCOPY) -O ihex -R .eeprom -R .fuse -R .lock -R .signature $< $@

clean:
	rm -rf $(BUILD)

.PHONY: all clean

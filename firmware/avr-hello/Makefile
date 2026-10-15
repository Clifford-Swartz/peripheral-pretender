# avr-hello: firmware for the AVR64EA48 Curiosity Nano (EV66E56A).
# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
#
# Builds with the avr-gcc in MPLAB XC8 and the AVR-Ex device pack:
#   make -C firmware/avr-hello [APP=hello|blinky|matrix|dgigpio]   (MSYS2 UCRT64)
# Applications: hello (first light, golden/avr64ea48-b1/hello-vcom.bin),
# blinky (LED0/SW0 demo for the web view, TCB0 interrupt), matrix (8x8 G
# Click on mikroBUS socket 1 of a Curiosity Nano Base).
# XC8 and DFP default to the MPLAB X install; override XC8= / DFP=, or
# PACKS= (the folder scripts/fetch_packs.py fills) to use its AVR-Ex DFP.

XC8    ?= /c/Program Files/Microchip/xc8/v4.00
ifdef PACKS
DFP    ?= $(PACKS)/Microchip/AVR-Ex_DFP/2.8.189
endif
DFP    ?= /c/Program Files/Microchip/MPLABX/v6.20/packs/Microchip/AVR-Ex_DFP/2.8.189
DEVICE := avr64ea48
CC     := "$(XC8)/avr/bin/avr-gcc"
OBJCOPY:= "$(XC8)/avr/bin/avr-objcopy"
BUILD  := build
APPS   := hello blinky matrix dgigpio
APP    ?= $(APPS)

CFLAGS := -mmcu=$(DEVICE) -B "$(DFP)/gcc/dev/$(DEVICE)" -isystem "$(DFP)/include" \
          -Os -std=gnu11 -Wall -Wextra -ffunction-sections -fdata-sections
LDFLAGS:= -Wl,--gc-sections

all: $(foreach a,$(APP),$(BUILD)/$(a).elf $(BUILD)/$(a).hex)

# hello is self-contained (first light); the others share the console
$(BUILD)/hello.elf: hello.c Makefile
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ hello.c

$(BUILD)/%.elf: %.c console.c board.h Makefile
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< console.c

# Program flash only: no fuses, lock bits or signature (harness.nano_run
# refuses anything else)
$(BUILD)/%.hex: $(BUILD)/%.elf
	$(OBJCOPY) -O ihex -R .eeprom -R .fuse -R .lock -R .signature $< $@

clean:
	rm -rf $(BUILD)

.PHONY: all clean

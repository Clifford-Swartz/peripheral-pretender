# Bare-metal hello image for PIC32CM5164JH01100 on the PIC32CM JH01
# Curiosity Pro (EV81X90A).  Build from an MSYS2 UCRT64 shell with "make";
# see README.md.
#
# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).

DEVICE   := pic32cm5164jh01100

# Read-only inputs: the packs MPLAB installs under ~/.mchp_packs
# (%USERPROFILE%\.mchp_packs on Windows); scripts/fetch_packs.py fetches them.
PACKS    ?= $(if $(USERPROFILE),$(subst \,/,$(USERPROFILE)),$(HOME))/.mchp_packs
DFP      ?= $(PACKS)/Microchip/PIC32CM-JH_DFP/1.3.221/PIC32CM-JH01
CMSIS    ?= $(PACKS)/ARM/CMSIS/6.1.0/CMSIS/Core/Include

CROSS    ?= arm-none-eabi-
CC       := $(CROSS)gcc
OBJCOPY  := $(CROSS)objcopy
OBJDUMP  := $(CROSS)objdump
SIZE     := $(CROSS)size

BUILD    := build
ELF      := $(BUILD)/hello.elf
LDSCRIPT := dfp/$(DEVICE)_flash.ld
SRCS     := src/main.c src/console.c src/sys_init.c dfp/startup_$(DEVICE).c
OBJS     := $(SRCS:%.c=$(BUILD)/%.o)

ARCH     := -mcpu=cortex-m0plus -mthumb
CPPFLAGS := -Isrc -isystem $(DFP)/include -isystem $(CMSIS)
CFLAGS   := $(ARCH) -std=gnu11 -Os -g -ffunction-sections -fdata-sections \
            -Wall -Wextra -MMD -MP
# The DFP startup replaces newlib's crt0 (-nostartfiles) but still calls
# __libc_init_array(), which newlib-nano provides and which calls _init():
# crti.o/crtn.o supply that, as the default startfiles would.  Nothing
# references a syscall, so nosys.specs is not needed.  The linker script
# has no ENTRY().
CRTI     := $(shell $(CC) $(ARCH) -print-file-name=crti.o)
CRTN     := $(shell $(CC) $(ARCH) -print-file-name=crtn.o)
LDFLAGS  := $(ARCH) -T$(LDSCRIPT) -nostartfiles --specs=nano.specs \
            -Wl,--entry=Reset_Handler -Wl,--gc-sections \
            -Wl,-Map=$(BUILD)/hello.map -Wl,--print-memory-usage

.PHONY: all check clean packs

all: $(ELF) $(BUILD)/hello.hex $(BUILD)/hello.lst
	$(SIZE) $(ELF)

$(ELF): $(OBJS) $(LDSCRIPT) Makefile
	$(CC) $(LDFLAGS) $(CRTI) $(OBJS) $(CRTN) -o $@

$(BUILD)/%.o: %.c Makefile | packs
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/hello.hex: $(ELF)
	$(OBJCOPY) -O ihex $< $@

$(BUILD)/hello.lst: $(ELF)
	$(OBJDUMP) -h -S $< > $@

check: $(ELF)
	CROSS=$(CROSS) sh check-elf.sh $(ELF) $(BUILD)/hello.map

clean:
	rm -rf $(BUILD)

packs:
	@test -f "$(DFP)/include/$(DEVICE).h" || \
	    { echo "DFP not found at $(DFP); set PACKS= or DFP=" >&2; exit 1; }
	@test -f "$(CMSIS)/core_cm0plus.h" || \
	    { echo "CMSIS not found at $(CMSIS); set PACKS= or CMSIS=" >&2; exit 1; }

-include $(OBJS:.o=.d)

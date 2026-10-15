# Bare-metal hello image for PIC32CM5164JH01100 on the PIC32CM JH01
# Curiosity Pro (EV81X90A).  Build from an MSYS2 UCRT64 shell with "make";
# see README.md.
#
# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).

# Board: boards/<BOARD>/ holds board.mk (device, DFP, VCOM vector), board.h
# and board.c (Harmony bring-up). make BOARD=pic32cm_mc00_cpro
BOARD    ?= pic32cm_jh01_cpro
include boards/$(BOARD)/board.mk

# Read-only inputs: the packs MPLAB installs under ~/.mchp_packs
# (%USERPROFILE%\.mchp_packs on Windows); scripts/fetch_packs.py fetches them.
PACKS    ?= $(if $(USERPROFILE),$(subst \,/,$(USERPROFILE)),$(HOME))/.mchp_packs
DFP      ?= $(PACKS)/Microchip/$(DFP_DIR)
CMSIS    ?= $(PACKS)/ARM/CMSIS/6.1.0/CMSIS/Core/Include

CROSS    ?= arm-none-eabi-
CC       := $(CROSS)gcc
OBJCOPY  := $(CROSS)objcopy
OBJDUMP  := $(CROSS)objdump
SIZE     := $(CROSS)size

# Application: the hello image (src/main.c) or a peripheral test
# (tests/<APP>.c): make APP=eic BOARD=pic32cm_mc00_cpro
APP      ?= hello
APP_SRC  := $(if $(filter hello,$(APP)),src/main.c,tests/$(APP).c)

BUILD    := build/$(BOARD)
ELF      := $(BUILD)/$(APP).elf
LDSCRIPT := dfp/$(DEVICE)_flash.ld
SRCS     := $(APP_SRC) src/console.c src/util.c boards/$(BOARD)/board.c \
            dfp/startup_$(DEVICE).c
OBJS     := $(SRCS:%.c=$(BUILD)/%.o)

# board.mk may set ARCH (core) and CMSIS_CORE (its CMSIS core header)
ARCH     ?= -mcpu=cortex-m0plus -mthumb
CMSIS_CORE ?= core_cm0plus.h
CPPFLAGS := -Isrc -Iboards/$(BOARD) -isystem "$(DFP)/include" -isystem "$(CMSIS)"
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
            -Wl,-Map=$(BUILD)/$(APP).map -Wl,--print-memory-usage

.PHONY: all check clean packs

all: $(ELF) $(BUILD)/$(APP).hex $(BUILD)/$(APP).lst
	$(SIZE) $(ELF)

$(ELF): $(OBJS) $(LDSCRIPT) Makefile boards/$(BOARD)/board.mk
	$(CC) $(LDFLAGS) $(CRTI) $(OBJS) $(CRTN) -o $@

$(BUILD)/%.o: %.c Makefile | packs
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/$(APP).hex: $(ELF)
	$(OBJCOPY) -O ihex $< $@

$(BUILD)/$(APP).lst: $(ELF)
	$(OBJDUMP) -h -S $< > $@

check: $(ELF)
	CROSS=$(CROSS) VCOM_IRQ=$(VCOM_IRQ) VCOM_HANDLER=$(VCOM_HANDLER) \
	    sh check-elf.sh $(ELF) $(BUILD)/$(APP).map

clean:
	rm -rf $(BUILD)

packs:
	@test -f "$(DFP)/include/$(DEVICE).h" || \
	    { echo "DFP not found at $(DFP); set PACKS= or DFP=" >&2; exit 1; }
	@test -f "$(CMSIS)/$(CMSIS_CORE)" || \
	    { echo "CMSIS not found at $(CMSIS); set PACKS= or CMSIS=" >&2; exit 1; }

-include $(OBJS:.o=.d)

# The Minish Cap NDS - scaffold build (ARM9 only, prebuilt ARM7 core).
# Self-contained: does not use devkitPro's ds_rules (which targets calico).
#
#   make [DEVKITARM=...] [NDS_PREFIX=...] [ARM7ELF=...] [NDSTOOL=...]
#
# NDS_PREFIX must contain include/ and lib/ with libnds9.a and libfat.a
# (libnds 1.8.x + libfat). ARM7ELF is any libnds ARM7 core; platform/nds/arm7
# holds a minimal one. Output: tmc-nds.nds
DEVKITARM ?= /opt/devkitpro/devkitARM
NDS_PREFIX ?= /opt/devkitpro/libnds
NDSTOOL ?= ndstool
ARM7ELF ?= arm7/default.elf
# Optional: path to your USA .gba. When set, every ROM-copy stub is verified byte-for-byte before
# being replaced by a link to the cart (see tools/gen_rom_symbols.py).
ROM ?=

PREFIX := $(DEVKITARM)/bin/arm-none-eabi-
CC := $(PREFIX)gcc
OBJCOPY := $(PREFIX)objcopy

ROOT := $(abspath ../..)
include engine_sources.mk
# data_const_stubs.c is replaced by generated linker symbols (Slot-2 maps the ROM) plus the few
# stubs that must stay in RAM.
ENGINE_SRCS := $(filter-out src/port/data_const_stubs.c,$(ENGINE_SRCS))

TARGET := tmc-nds
BUILD := build
NDS_SRCS := $(wildcard source/*.c)
# Object names mirror source paths so same-named engine files cannot collide.
ENGINE_OBJS := $(patsubst %.c,$(BUILD)/engine/%.o,$(ENGINE_SRCS))
SHARED_OBJS := $(patsubst %.c,$(BUILD)/shared/%.o,$(SHARED3DS_SRCS))
NDS_OBJS := $(patsubst source/%.c,$(BUILD)/nds/%.o,$(NDS_SRCS))
NDS_ENGINE_SRCS := $(wildcard engine/*.c)
NDS_ENGINE_OBJS := $(patsubst engine/%.c,$(BUILD)/nds_engine/%.o,$(NDS_ENGINE_SRCS))
GEN_OBJS := $(BUILD)/gen/kept_stubs.o
OBJS := $(NDS_OBJS) $(NDS_ENGINE_OBJS) $(SHARED_OBJS) $(ENGINE_OBJS) $(GEN_OBJS)

ARCH := -march=armv5te -mtune=arm946e-s -mthumb -mthumb-interwork
COMMON := $(ARCH) -ffunction-sections -fdata-sections -DARM9 -DTMC_NDS -I$(NDS_PREFIX)/include
# The engine shares the 32-bit/no-SDL paths of the 3DS port (TMC_3DS) and consumes
# its SDK-free Platform3DS_* API; platform_nds.c implements it on libnds.
ENGINE_FLAGS := -std=gnu11 -Os -w -fno-strict-aliasing -fwrapv -fno-strict-overflow -include region.h \
    -DTMC_3DS -DPC_PORT -DNON_MATCHING -DUSE_HDMA -DMULTI_REGION -DUSA -DENGLISH -DREVISION=0 \
    -DMODE1_GBA_WIDTH=240 -DMODE1_GBA_HEIGHT=160 \
    -I$(ROOT) -I$(ROOT)/src/engine/include -I$(ROOT)/src/port -I$(ROOT)/src/port/ppu/include \
    -I$(ROOT)/src/third_party/agbplay_core -I$(ROOT)/src/offsets/USA -I$(ROOT)/platform/3ds/source -I$(CURDIR)/compat
CFLAGS := -g -Wall -O2 $(COMMON) -Isource -I$(ROOT)/platform/3ds/source -I$(CURDIR)/compat
LDFLAGS := -specs=ds_arm9.specs -g $(ARCH) -Wl,-Map,$(BUILD)/$(TARGET).map -Wl,--gc-sections -Wl,-u,AgbMain

all: $(TARGET).nds

$(BUILD)/nds/%.o: source/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -MMD -c $< -o $@

$(BUILD)/engine/%.o: $(ROOT)/%.c
	@mkdir -p $(@D)
	$(CC) $(COMMON) $(ENGINE_FLAGS) -MMD -c $< -o $@

$(BUILD)/gen/rom_symbols.ld $(BUILD)/gen/kept_stubs.c: $(ROOT)/src/port/data_const_stubs.c tools/gen_rom_symbols.py
	@mkdir -p $(@D)
	python3 tools/gen_rom_symbols.py $< $(BUILD)/gen/rom_symbols.ld $(BUILD)/gen/kept_stubs.c $(if $(ROM),--rom "$(ROM)")

$(BUILD)/gen/kept_stubs.o: $(BUILD)/gen/kept_stubs.c
	$(CC) $(COMMON) $(ENGINE_FLAGS) -c $< -o $@

$(BUILD)/nds_engine/%.o: engine/%.c
	@mkdir -p $(@D)
	$(CC) $(COMMON) $(ENGINE_FLAGS) -Isource -MMD -c $< -o $@

$(BUILD)/shared/%.o: $(ROOT)/platform/3ds/source/%.c
	@mkdir -p $(@D)
	$(CC) $(COMMON) $(ENGINE_FLAGS) -MMD -c $< -o $@

$(TARGET).elf: $(OBJS) $(BUILD)/gen/rom_symbols.ld
	@printf "%s\n" $(OBJS) $(BUILD)/gen/rom_symbols.ld > $(BUILD)/objs.rsp
	$(CC) $(LDFLAGS) @$(BUILD)/objs.rsp -L$(NDS_PREFIX)/lib -lfat -lnds9 -lm -o $@
	@$(DEVKITARM)/bin/arm-none-eabi-size $@

$(TARGET).nds: $(TARGET).elf $(ARM7ELF)
	$(NDSTOOL) -c $@ -9 $(TARGET).elf -7 $(ARM7ELF) -b icon.bmp "The Minish Cap NDS;engine linked;"

clean:
	rm -rf $(BUILD) $(TARGET).elf $(TARGET).nds

-include $(OBJS:.o=.d)
.PHONY: all clean

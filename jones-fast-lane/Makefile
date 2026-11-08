# Jones in the Fast Lane - NDS port (see BUILDING.md)
#   make          -> build/jones.nds         (floppy release, needs GAMEDIR)
#   make cd       -> build/jones-cdrom.nds   (CD release with voices, needs CDDIR + GAMEDIR)
#   make mac      -> build/mac/restest (host test tool)
#   make test     -> run host resource test
#
# The game's files are not part of this project. Point the build at your
# own copy, e.g.:  make GAMEDIR=~/games/jones-dos CDDIR=~/games/jones-cd
# Everything below marked ?= can be overridden on the command line.

DKP      ?= $(CURDIR)/toolchain/dkp
DEVKITARM ?= $(firstword $(wildcard $(DKP)/devkitARM) /opt/devkitpro/devkitARM)
BIN      := $(DEVKITARM)/bin
CC9      := $(BIN)/arm-none-eabi-gcc
NDSTOOL  ?= $(firstword $(wildcard $(CURDIR)/toolchain/bin/ndstool $(DKP)/tools/bin/ndstool $(DEVKITPRO)/tools/bin/ndstool /opt/devkitpro/tools/bin/ndstool) ndstool)
CALICO   := $(DKP)/calico
LIBNDS   := $(DKP)/libnds
GAMEDIR  ?= $(CURDIR)/Jones-in-the-Fast-Lane_DOS_EN
ARM7ELF  ?= $(CALICO)/bin/ds7_ragdoll.elf

ENGINE   := src/engine/resource.c src/engine/decompress.c src/engine/gfx.c
NDS_SRC  := src/nds/main.c $(ENGINE) src/engine/vm.c src/engine/kernel.c src/engine/kgraphics.c src/engine/ksound.c src/engine/ksave.c src/engine/music.c src/engine/topstatus.c src/engine/voice.c src/engine/subtitle.c src/engine/balloon.c src/engine/locpanel.c
NDS_ASM  := src/nds/gamedata.s

ARCH9    := -march=armv5te -mtune=arm946e-s -mthumb
CFLAGS9  := $(ARCH9) -O2 -Wall -ffunction-sections -fdata-sections \
            -D__NDS__ -DARM9 -I$(LIBNDS)/include -I$(CALICO)/include
LDFLAGS9 := $(ARCH9) -specs=$(CALICO)/share/ds9.specs \
            -L$(LIBNDS)/lib -L$(CALICO)/lib
LIBS9    := -lfat -lnds9 -lcalico_ds9 -lm

OBJDIR   := build/nds
OBJS     := $(patsubst src/%.c,$(OBJDIR)/%.o,$(NDS_SRC)) $(OBJDIR)/nds/gamedata.o

.PHONY: all cd mac test gfxtest scirun musicwav clean
all: build/jones.nds

# Friendly errors when the game's files are not where the build expects them.
FLOPPY_FILES := resource.map resource.001 resource.002
CD_FILES     := resource.map resource.001 audio001.map audio001.002
$(addprefix $(GAMEDIR)/,$(FLOPPY_FILES)):
	@echo "error: missing $@" >&2
	@echo "  Copy resource.map, resource.001 and resource.002 from your floppy (DOS) copy" >&2
	@echo "  of the game into $(GAMEDIR), or run: make GAMEDIR=/path/to/your/copy" >&2
	@echo "  See BUILDING.md." >&2
	@false
$(addprefix $(CDDIR)/,$(filter-out resource.map resource.001,$(CD_FILES))) $(CDDIR)/resource.map $(CDDIR)/resource.001:
	@echo "error: missing $@" >&2
	@echo "  Copy resource.map, resource.001, audio001.map and audio001.002 from your CD copy" >&2
	@echo "  of the game into $(CDDIR), or run: make cd CDDIR=/path/to/your/cd" >&2
	@echo "  See BUILDING.md." >&2
	@false

# CD-ROM release with character voices: speech files go into the ROM's NitroFS
CDDIR    ?= $(CURDIR)/Jones3x/CD
CDOBJDIR := build/nds-cd
CDOBJS   := $(patsubst src/%.c,$(CDOBJDIR)/%.o,$(NDS_SRC) src/nds/nitrorom_common.c) $(CDOBJDIR)/nds/gamedata.o
cd: build/jones-cdrom.nds

build/jones-cdrom.nds: build/jones-cdrom.elf $(CDDIR)/audio001.map $(CDDIR)/audio001.002 $(CDDIR)/resource.001 build/subtitles.bin
	@mkdir -p build/cdfs
	cp build/subtitles.bin build/cdfs/subtitles.bin
	ln -f $(CDDIR)/resource.001 build/cdfs/resource.001
	ln -f $(CDDIR)/audio001.map build/cdfs/audio001.map
	ln -f $(CDDIR)/audio001.002 build/cdfs/audio001.002
	$(NDSTOOL) -c $@ -9 $< -7 $(ARM7ELF) -b $(CALICO)/share/nds-icon.bmp \
	  "Jones in the Fast Lane CD;Sierra On-Line 1991;NDS port (voices)" -d build/cdfs

# subtitles for the CD speech, from the floppy release's text
build/subtitles.bin: tools/mksubs.py build/mac/textdump $(CDDIR)/audio001.map $(addprefix $(GAMEDIR)/,$(FLOPPY_FILES))
	python3 tools/mksubs.py build/mac/textdump $(GAMEDIR) $(CDDIR)/audio001.map $@

build/mac/textdump: tools/textdump.c $(ENGINE) src/engine/resource.h
	@mkdir -p $(dir $@)
	cc -O2 -Wall -o $@ tools/textdump.c $(ENGINE)

build/jones-cdrom.elf: $(CDOBJS)
	$(CC9) $(LDFLAGS9) $^ $(LIBS9) -o $@

$(CDOBJDIR)/%.o: src/%.c $(wildcard src/engine/*.h)
	@mkdir -p $(dir $@)
	$(CC9) $(CFLAGS9) -DCDROM $(CDEXTRA) -c $< -o $@

$(CDOBJDIR)/nds/gamedata.o: $(NDS_ASM) $(CDDIR)/resource.map
	@mkdir -p $(dir $@)
	$(CC9) $(ARCH9) -x assembler-with-cpp -DCDROM -DGAMEDIR=$(CDDIR) -c $< -o $@

build/jones.nds: build/jones.elf
	$(NDSTOOL) -c $@ -9 $< -7 $(ARM7ELF) -b $(CALICO)/share/nds-icon.bmp \
	  "Jones in the Fast Lane;Sierra On-Line 1991;NDS port"

build/jones.elf: $(OBJS)
	$(CC9) $(LDFLAGS9) $^ $(LIBS9) -o $@

# ITCM code is compiled as ARM (faster than Thumb when running from ITCM)
$(OBJDIR)/%.itcm.o: src/%.itcm.c $(wildcard src/nds/*.h)
	@mkdir -p $(dir $@)
	$(CC9) $(CFLAGS9) -marm -O3 -c $< -o $@

$(OBJDIR)/%.o: src/%.c $(wildcard src/engine/*.h)
	@mkdir -p $(dir $@)
	$(CC9) $(CFLAGS9) $(NDSEXTRA) -c $< -o $@

$(OBJDIR)/nds/gamedata.o: $(NDS_ASM) $(GAMEDIR)/resource.map $(GAMEDIR)/resource.001 $(GAMEDIR)/resource.002
	@mkdir -p $(dir $@)
	$(CC9) $(ARCH9) -x assembler-with-cpp -DGAMEDIR=$(GAMEDIR) -c $< -o $@

mac: build/mac/restest
build/mac/restest: tools/restest.c $(ENGINE) src/engine/resource.h
	@mkdir -p $(dir $@)
	cc -O2 -Wall -Wextra -o $@ tools/restest.c $(ENGINE)

test: build/mac/restest
	./build/mac/restest $(GAMEDIR)

clean:
	rm -rf build/nds build/jones.elf build/jones.nds build/mac build/nds-cd build/cdfs build/jones-cdrom.elf build/jones-cdrom.nds

gfxtest: build/mac/gfxtest
	@mkdir -p build/gfx
	./build/mac/gfxtest $(GAMEDIR) build/gfx
	@cd build/gfx && for f in *.bmp; do sips -s format png "$$f" --out "$${f%.bmp}.png" >/dev/null && rm "$$f"; done
	@echo "PNGs written to build/gfx/"
build/mac/gfxtest: tools/gfxtest.c $(ENGINE) src/engine/resource.h src/engine/gfx.h
	@mkdir -p $(dir $@)
	cc -O2 -Wall -Wextra -o $@ tools/gfxtest.c $(ENGINE)

ENGINE_ALL := $(ENGINE) src/engine/vm.c src/engine/kernel.c src/engine/kgraphics.c src/engine/ksound.c src/engine/ksave.c src/engine/music.c src/engine/topstatus.c src/engine/voice.c src/engine/subtitle.c src/engine/balloon.c src/engine/locpanel.c
scirun: build/mac/scirun
build/mac/scirun: tools/scirun.c $(ENGINE_ALL) $(wildcard src/engine/*.h)
	@mkdir -p $(dir $@)
	cc -O1 -g -Wall -Wextra -Wno-unused-parameter -o $@ tools/scirun.c $(ENGINE_ALL) -lm

musicwav: build/mac/musicwav
build/mac/musicwav: tools/musicwav.c $(ENGINE) src/engine/music.c src/engine/music.h
	@mkdir -p $(dir $@)
	cc -O2 -Wall -Wextra -o $@ tools/musicwav.c $(ENGINE) src/engine/music.c -lm

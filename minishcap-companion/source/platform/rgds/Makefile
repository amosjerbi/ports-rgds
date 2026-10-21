# The Minish Cap RG DS - native aarch64 Linux build (ROCKNIX). Run via ./build.sh (Docker) or directly
# in an arm64 Linux environment with gcc and libsdl2-dev.
ROOT := $(abspath ../..)
include engine_sources.mk

CC ?= gcc
TARGET := build/tmc-rgds
BUILD := build

RGDS_SRCS_ALL := $(wildcard source/*.c)
RGDS_SRCS := $(filter-out source/main_rgds.c,$(RGDS_SRCS_ALL))
BOTTOM_SRCS := $(wildcard companion/source/*.c)
BOTTOM_ENGINE_SRCS := $(wildcard companion/engine/*.c)
RGDS_ENGINE_SRCS := $(wildcard engine/*.c)
ENGINE_OBJS := $(patsubst %.c,$(BUILD)/engine/%.o,$(ENGINE_SRCS))
SHARED_OBJS := $(patsubst %.c,$(BUILD)/shared/%.o,$(SHARED3DS_SRCS))
RGDS_OBJS := $(patsubst source/%.c,$(BUILD)/rgds/%.o,$(RGDS_SRCS))
RGDS_ENGINE_OBJS := $(patsubst engine/%.c,$(BUILD)/rgds_engine/%.o,$(RGDS_ENGINE_SRCS))
MAIN_OBJ := $(BUILD)/rgds/main_rgds.o
BOTTOM_OBJS := $(patsubst companion/source/%.c,$(BUILD)/bottom/%.o,$(BOTTOM_SRCS)) \
               $(patsubst companion/engine/%.c,$(BUILD)/bottom_engine/%.o,$(BOTTOM_ENGINE_SRCS))
OBJS := $(MAIN_OBJ) $(RGDS_OBJS) $(RGDS_ENGINE_OBJS) $(SHARED_OBJS) $(ENGINE_OBJS)
BOTTOM_ALL_OBJS := $(BOTTOM_OBJS) $(RGDS_OBJS) $(RGDS_ENGINE_OBJS) $(SHARED_OBJS) $(ENGINE_OBJS)

# The engine runs the maintained native-port paths (TMC_3DS) on a 64-bit target; platform_rgds.c
# implements the Platform3DS_* API on SDL2. SDL2 is used only in source/, never in engine TUs.
ENGINE_FLAGS := -std=gnu11 -O2 -w -fPIC -fno-strict-aliasing -fwrapv -fno-strict-overflow -include region.h \
    -DTMC_RGDS -DTMC_3DS -DPC_PORT -DNON_MATCHING -DUSE_HDMA -DMULTI_REGION -DUSA -DENGLISH -DREVISION=0 \
    -DMODE1_GBA_WIDTH=240 -DMODE1_GBA_HEIGHT=160 \
    -I$(ROOT) -I$(ROOT)/src/engine/include -I$(ROOT)/src/port -I$(ROOT)/src/port/ppu/include \
    -I$(ROOT)/src/third_party/agbplay_core -I$(ROOT)/src/offsets/USA -I$(ROOT)/platform/3ds/source \
    -I$(CURDIR)/compat -Isource -I$(CURDIR)/companion/source -I$(CURDIR)/companion
SDL_FLAGS := -std=gnu11 -O2 -Wall -fPIC -I$(ROOT)/platform/3ds/source -Isource $(shell sdl2-config --cflags)
LDFLAGS := -Wl,--gc-sections
LIBS := $(shell sdl2-config --libs) -lm -lpthread

all: $(TARGET) $(BUILD)/tmc-bottom

$(BUILD)/rgds/%.o: source/%.c
	@mkdir -p $(@D)
	$(CC) $(SDL_FLAGS) -MMD -c $< -o $@

$(BUILD)/bottom/%.o: companion/source/%.c
	@mkdir -p $(@D)
	$(CC) $(SDL_FLAGS) -I$(CURDIR)/companion/source -MMD -c $< -o $@

$(BUILD)/bottom_engine/%.o: companion/engine/%.c
	@mkdir -p $(@D)
	$(CC) $(ENGINE_FLAGS) -MMD -c $< -o $@

$(BUILD)/rgds_engine/%.o: engine/%.c
	@mkdir -p $(@D)
	$(CC) $(ENGINE_FLAGS) -MMD -c $< -o $@

$(BUILD)/engine/%.o: $(ROOT)/%.c
	@mkdir -p $(@D)
	$(CC) $(ENGINE_FLAGS) -MMD -c $< -o $@

$(BUILD)/shared/%.o: $(ROOT)/platform/3ds/source/%.c
	@mkdir -p $(@D)
	$(CC) $(ENGINE_FLAGS) -MMD -c $< -o $@

$(TARGET): $(OBJS)
	@printf "%s\n" $(OBJS) > $(BUILD)/objs.rsp
	$(CC) $(LDFLAGS) -Wl,-u,AgbMain @$(BUILD)/objs.rsp $(LIBS) -o $@

$(BUILD)/tmc-bottom: $(BOTTOM_ALL_OBJS)
	@printf "%s\n" $(BOTTOM_ALL_OBJS) > $(BUILD)/bottom.rsp
	$(CC) $(LDFLAGS) @$(BUILD)/bottom.rsp $(LIBS) -o $@

clean:
	rm -rf $(BUILD)

-include $(OBJS:.o=.d) $(BOTTOM_OBJS:.o=.d)
.PHONY: all clean

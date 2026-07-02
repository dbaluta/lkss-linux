# Makefile for the LKSS hackathon LVGL apps (standalone, not kbuild)
#
# Cross-compiles LVGL, the warm-up samples and the project skeletons
# for the i.MX93 FRDM board:
#
#   git clone --depth 1 --branch v9.3.0 https://github.com/lvgl/lvgl.git
#   make -j$(nproc)
#
# Binaries land in build/, named after the app (build/pong, build/leds,
# ...) so they keep a suggestive name when copied to the board.
# Build a single app with e.g. "make leds". Native build: make CROSS_COMPILE=

CROSS_COMPILE ?= aarch64-linux-gnu-
CC := $(CROSS_COMPILE)gcc
AR := $(CROSS_COMPILE)ar

LVGL  := lvgl
BUILD := build

# Warm-up samples: complete, working - run and read these first.
SAMPLES := hello colors flag dot paddle bounce jump blink leds bar menu chart
# Project skeletons: <name>/<name>.c with TODOs for you to fill in.
DEMOS   := pong snake breakout tetris watch weather sysmon

APPS := $(SAMPLES) $(DEMOS)

# -DLV_CONF_INCLUDE_SIMPLE makes LVGL include our lv_conf.h from -I.
CFLAGS := -O2 -Wall -DLV_CONF_INCLUDE_SIMPLE -I. -Icommon -I$(LVGL)
LDLIBS := -lm

# All LVGL C sources; disabled features compile to empty objects
LVGL_SRC   := $(shell find $(LVGL)/src -name '*.c')
COMMON_SRC := common/hal.c
LIB_OBJ    := $(patsubst %.c,$(BUILD)/%.o,$(LVGL_SRC) $(COMMON_SRC))

all: $(APPS)

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/liblvgl.a: $(LIB_OBJ)
	$(AR) rcs $@ $^

$(APPS): $(BUILD)/liblvgl.a
	$(CC) $(CFLAGS) $@/$@.c $(BUILD)/liblvgl.a $(LDLIBS) -o $(BUILD)/$@

clean:
	rm -rf $(BUILD)

.PHONY: all clean  $(APPS)

# SPDX-License-Identifier: GPL-2.0
#
# Userspace Makefile - Lab 3 display demo applications
#
# Cross-compile for the i.MX93 ARM64 target:
#
#   make CROSS_COMPILE=aarch64-linux-gnu-
#
CROSS_COMPILE ?= aarch64-linux-gnu-
CC             = $(CROSS_COMPILE)gcc
CFLAGS         = -O2 -Wall -Wextra -std=c99

PROGS = st7789_scene pong

all: $(PROGS)

st7789_scene: st7789_scene.c
	$(CC) $(CFLAGS) -o $@ $^

pong: pong.c
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f $(PROGS)

.PHONY: all clean

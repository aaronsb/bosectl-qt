# Builds build/tools/fakeinput, a CLI that injects pointer and keyboard events
# into a KWin Wayland session through org_kde_kwin_fake_input. Copied from
# kwin-canvas; the nest (dev/nest.sh) runs KWin without permission checks, so
# no .desktop registration is needed there.
#
#   make -C tools          build build/tools/fakeinput
#   make -C tools clean    remove build/tools
#
# fakeinput.c and keys.h are GPL-2.0-or-later and the protocol is
# LGPL-2.1-or-later (LICENSES/). The tool is a separate test program; nothing
# of it links into bosectl-qt.

TOOLS := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
TOP   := $(abspath $(TOOLS)/..)
OUT   := $(TOP)/build/tools

PKG_CONFIG ?= pkg-config
WAYLAND_SCANNER ?= wayland-scanner
CC ?= cc

WL_CFLAGS := $(shell $(PKG_CONFIG) --cflags wayland-client)
WL_LIBS   := $(shell $(PKG_CONFIG) --libs wayland-client)

CFLAGS  ?= -O2 -g
CFLAGS  += -std=c11 -Wall -Wextra -I$(OUT) -I$(TOOLS) $(WL_CFLAGS)
LDLIBS  += $(WL_LIBS)

PROTO   := $(TOOLS)/protocols/fake-input.xml
PROTO_H := $(OUT)/fake-input-client-protocol.h
PROTO_C := $(OUT)/fake-input-protocol.c
BIN     := $(OUT)/fakeinput

.PHONY: all clean

all: $(BIN)

$(OUT):
	mkdir -p $(OUT)

$(PROTO_H): $(PROTO) | $(OUT)
	$(WAYLAND_SCANNER) client-header $< $@

$(PROTO_C): $(PROTO) | $(OUT)
	$(WAYLAND_SCANNER) private-code $< $@

$(OUT)/fakeinput.o: $(TOOLS)/fakeinput.c $(TOOLS)/keys.h $(PROTO_H) | $(OUT)
	$(CC) $(CFLAGS) -c -o $@ $<

$(OUT)/fake-input-protocol.o: $(PROTO_C) | $(OUT)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BIN): $(OUT)/fakeinput.o $(OUT)/fake-input-protocol.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

clean:
	rm -rf $(OUT)

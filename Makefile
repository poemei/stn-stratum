# Copyright (c) 2026 STN-Labz. All rights reserved.
#
# STN-Stratum - Linux Build
#
# Usage:
#   make configure
#   make build
#   sudo make install
#
# Optional:
#   make clean
#   sudo make uninstall
#
# Installation prefix may be overridden:
#   sudo make install PREFIX=/opt/stn-stratum

CC      ?= gcc
PREFIX  ?= /usr/local
BINDIR  ?= $(PREFIX)/bin

BUILD_DIR := build
TARGET    := $(BUILD_DIR)/stn-stratum

CPPFLAGS := -D_POSIX_C_SOURCE=200809L -Iincludes -Iplatforms
CFLAGS   := -std=c17 -Wall -Wextra -Wpedantic -Werror -O2

COMMON_SOURCES := \
	src/main.c \
	src/stn_stratum_server.c \
	src/stn_rpc_client.c \
	src/stn_chain_config.c \
	src/stn_chain_rpc_adapter.c \
	src/stn_stratum.c

LINUX_SOURCES := \
	platforms/linux/stn_rpc_linux.c

SOURCES := $(COMMON_SOURCES) $(LINUX_SOURCES)

OBJECTS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(SOURCES))

.PHONY: all configure build install uninstall clean

all: build

configure:
	@echo "Configuring STN-Stratum for Linux..."
	@echo "Compiler: $(CC)"
	@echo "Prefix:   $(PREFIX)"
	@echo "Binary:   $(BINDIR)/stn-stratum"
	@mkdir -p $(BUILD_DIR)
	@echo "Configuration complete."

build: configure $(TARGET)
	@echo "STN-Stratum build complete:"
	@echo "  $(TARGET)"

$(TARGET): $(OBJECTS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(OBJECTS) -o $@

$(BUILD_DIR)/src/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/platforms/linux/%.o: platforms/linux/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

install: build
	@echo "Installing STN-Stratum..."
	install -d "$(DESTDIR)$(BINDIR)"
	install -m 0755 "$(TARGET)" "$(DESTDIR)$(BINDIR)/stn-stratum"
	@echo "Installed:"
	@echo "  $(DESTDIR)$(BINDIR)/stn-stratum"

uninstall:
	@echo "Removing STN-Stratum..."
	rm -f "$(DESTDIR)$(BINDIR)/stn-stratum"
	@echo "STN-Stratum executable removed."

clean:
	rm -rf "$(BUILD_DIR)"
	@echo "Build files removed."
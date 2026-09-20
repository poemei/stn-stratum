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

CONFIGDIR ?= /etc/stn-stratum
LOGDIR    ?= /var/log/stratum

BUILD_DIR := build
TARGET    := $(BUILD_DIR)/stn-stratum

CONFIG_SOURCE := config/chains.json
CONFIG_TARGET := $(CONFIGDIR)/config.json
LOG_TARGET    := $(LOGDIR)/stratum.log

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
	@echo "Config:   $(CONFIG_TARGET)"
	@echo "Log:      $(LOG_TARGET)"
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
	install -d "$(DESTDIR)$(CONFIGDIR)"
	install -d -o stnchain -g stnchain -m 0755 "$(DESTDIR)$(LOGDIR)"

	install -m 0755 "$(TARGET)" \
		"$(DESTDIR)$(BINDIR)/stn-stratum"

	install -m 0644 "$(CONFIG_SOURCE)" \
		"$(DESTDIR)$(CONFIG_TARGET)"

	touch "$(DESTDIR)$(LOG_TARGET)"
	chown stnchain:stnchain "$(DESTDIR)$(LOG_TARGET)"
	chmod 0644 "$(DESTDIR)$(LOG_TARGET)"

	@echo "Installed:"
	@echo "  $(DESTDIR)$(BINDIR)/stn-stratum"
	@echo "  $(DESTDIR)$(CONFIG_TARGET)"
	@echo "  $(DESTDIR)$(LOG_TARGET)"

uninstall:
	@echo "Removing STN-Stratum..."

	rm -f "$(DESTDIR)$(BINDIR)/stn-stratum"
	rm -f "$(DESTDIR)$(CONFIG_TARGET)"

	@echo "Removed:"
	@echo "  $(DESTDIR)$(BINDIR)/stn-stratum"
	@echo "  $(DESTDIR)$(CONFIG_TARGET)"
	@echo "Preserved:"
	@echo "  $(DESTDIR)$(LOG_TARGET)"

clean:
	rm -rf "$(BUILD_DIR)"
	@echo "Build files removed."
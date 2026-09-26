# Copyright (c) 2026 STN-Labz. All rights reserved.
#
# STN-Stratum - Linux Build
#
# Usage:
#   make configure
#   make build
#   sudo make install
#   sudo make install-service
#
# Optional:
#   make clean
#   sudo make uninstall
#   sudo make uninstall-service
#
# Installation prefix may be overridden:
#   sudo make install PREFIX=/opt/stn-stratum

CC      ?= gcc
PREFIX  ?= /usr/local
BINDIR  ?= $(PREFIX)/bin

CONFIGDIR  ?= /etc/stn-stratum
LOGDIR     ?= /var/log/stratum
SERVICEDIR ?= /etc/systemd/system

BUILD_DIR := build
TARGET    := $(BUILD_DIR)/stn-stratum

CONFIG_SOURCE := config/chains.json
CONFIG_TARGET := $(CONFIGDIR)/config.json

LOG_TARGET := $(LOGDIR)/stratum.log

SERVICE_SOURCE := platforms/linux/stn-stratum.service
SERVICE_TARGET := $(SERVICEDIR)/stn-stratum.service

CPPFLAGS := -D_POSIX_C_SOURCE=200809L -Iincludes -Iplatforms
CFLAGS   := -std=c17 -Wall -Wextra -Wpedantic -Werror -O2

COMMON_SOURCES := \
	src/main.c \
	src/stn_stratum_server.c \
	src/stn_rpc_client.c \
	src/stn_chain_config.c \
	src/stn_chain_rpc_adapter.c \
	src/stn_stratum.c \
	src/stn_share_verify.c \
	src/stn_miner_job.c

LINUX_SOURCES := \
	platforms/linux/stn_rpc_linux.c

SOURCES := $(COMMON_SOURCES) $(LINUX_SOURCES)

OBJECTS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(SOURCES))

.PHONY: all configure build install install-service \
	uninstall uninstall-service clean test-miner-job test-telemetry

all: build

configure:
	@echo "Configuring STN-Stratum for Linux..."
	@echo "Compiler: $(CC)"
	@echo "Prefix:   $(PREFIX)"
	@echo "Binary:   $(BINDIR)/stn-stratum"
	@echo "Config:   $(CONFIG_TARGET)"
	@echo "Log:      $(LOG_TARGET)"
	@echo "Service:  $(SERVICE_TARGET)"
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
	install -d -o root -g stnchain -m 0750 "$(DESTDIR)$(CONFIGDIR)"
	install -d -o stnchain -g stnchain -m 0755 "$(DESTDIR)$(LOGDIR)"

	install -o root -g root -m 0755 "$(TARGET)" \
		"$(DESTDIR)$(BINDIR)/stn-stratum"

	install -o root -g stnchain -m 0640 "$(CONFIG_SOURCE)" \
		"$(DESTDIR)$(CONFIG_TARGET)"

	touch "$(DESTDIR)$(LOG_TARGET)"
	chown stnchain:stnchain "$(DESTDIR)$(LOG_TARGET)"
	chmod 0644 "$(DESTDIR)$(LOG_TARGET)"

	@echo "Installed:"
	@echo "  $(DESTDIR)$(BINDIR)/stn-stratum"
	@echo "  $(DESTDIR)$(CONFIG_TARGET)"
	@echo "  $(DESTDIR)$(LOG_TARGET)"

install-service:
	@echo "Installing STN-Stratum service..."

	install -d "$(DESTDIR)$(SERVICEDIR)"
	install -o root -g root -m 0644 "$(SERVICE_SOURCE)" \
		"$(DESTDIR)$(SERVICE_TARGET)"

	@if [ -z "$(DESTDIR)" ]; then \
		systemctl daemon-reload; \
	fi

	@echo "Installed:"
	@echo "  $(DESTDIR)$(SERVICE_TARGET)"
	@echo ""
	@echo "Enable and start with:"
	@echo "  systemctl enable --now stn-stratum.service"

uninstall-service:
	@echo "Removing STN-Stratum service..."

	@if [ -z "$(DESTDIR)" ]; then \
		systemctl disable --now stn-stratum.service 2>/dev/null || true; \
	fi

	rm -f "$(DESTDIR)$(SERVICE_TARGET)"

	@if [ -z "$(DESTDIR)" ]; then \
		systemctl daemon-reload; \
	fi

	@echo "STN-Stratum service removed."

uninstall:
	@echo "Removing STN-Stratum..."

	rm -f "$(DESTDIR)$(BINDIR)/stn-stratum"
	rm -f "$(DESTDIR)$(CONFIG_TARGET)"

	@echo "Removed:"
	@echo "  $(DESTDIR)$(BINDIR)/stn-stratum"
	@echo "Preserved:"
	@echo "  $(DESTDIR)$(LOG_TARGET)"

clean:
	rm -rf "$(BUILD_DIR)"
	@echo "Build files removed."

test-miner-job: configure
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_miner_job.c src/stn_miner_job.c -o $(BUILD_DIR)/test-miner-job
	$(BUILD_DIR)/test-miner-job

test-telemetry: configure
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_telemetry.c -o $(BUILD_DIR)/test-telemetry
	$(BUILD_DIR)/test-telemetry

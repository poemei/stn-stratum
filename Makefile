# Copyright (c) 2026 STN-Labz. All rights reserved.
#
# STN Chain - Linux Build
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
#   sudo make install PREFIX=/opt/stn-chain

CC      ?= gcc
PREFIX  ?= /usr/local
BINDIR  ?= $(PREFIX)/bin

BUILD_DIR := build
TARGET    := $(BUILD_DIR)/stn-chain

CPPFLAGS := -D_POSIX_C_SOURCE=200809L -Iincludes -Iplatforms
CFLAGS   := -std=c17 -Wall -Wextra -Wpedantic -Werror -O2

COMMON_SOURCES := \
	src/main.c \
	src/stn_authority.c \
	src/stn_block.c \
	src/stn_chain.c \
	src/stn_fork.c \
	src/stn_identity.c \
	src/stn_intelligence.c \
	src/stn_lifecycle.c \
	src/stn_mining.c \
	src/stn_node_service.c \
	src/stn_peer.c \
	src/stn_pending.c \
	src/stn_pow.c \
	src/stn_record.c \
	src/stn_replay.c \
	src/stn_rpc.c \
	src/stn_storage.c \
	src/stn_transaction.c \
	src/stn_validation.c

CRYPTO_SOURCES := \
	src/crypto/ed25519_donna/ed25519.c \
	src/crypto/ed25519_donna/ed25519_provider.c

LINUX_SOURCES := \
	platforms/linux/stn_app_linux.c \
	platforms/linux/stn_peer_linux.c \
	platforms/linux/stn_storage_linux.c \
	platforms/linux/stn_sha256.c

SOURCES := $(COMMON_SOURCES) $(CRYPTO_SOURCES) $(LINUX_SOURCES)

OBJECTS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(SOURCES))

.PHONY: all configure build install uninstall clean

all: build

configure:
	@echo "Configuring STN Chain for Linux..."
	@echo "Compiler: $(CC)"
	@echo "Prefix:   $(PREFIX)"
	@echo "Binary:   $(BINDIR)/stn-chain"
	@mkdir -p $(BUILD_DIR)
	@echo "Configuration complete."

build: configure $(TARGET)
	@echo "STN Chain build complete:"
	@echo "  $(TARGET)"

$(TARGET): $(OBJECTS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(OBJECTS) -o $@

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

install: build
	@echo "Installing STN Chain..."
	install -d "$(DESTDIR)$(BINDIR)"
	install -m 0755 "$(TARGET)" "$(DESTDIR)$(BINDIR)/stn-chain"
	@echo "Installed:"
	@echo "  $(DESTDIR)$(BINDIR)/stn-chain"

uninstall:
	@echo "Removing STN Chain..."
	rm -f "$(DESTDIR)$(BINDIR)/stn-chain"
	@echo "STN Chain executable removed."

clean:
	rm -rf "$(BUILD_DIR)"
	@echo "Build files removed."
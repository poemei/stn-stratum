/* Copyright (c) 2026 STN-Labz. All rights reserved. */
#ifndef STN_CHAIN_CONFIG_H
#define STN_CHAIN_CONFIG_H

#include <stddef.h>
#include <stdint.h>

typedef struct stn_chain_config_entry {
    char *host;
    uint16_t port;
} stn_chain_config_entry;

typedef struct stn_chain_config {
    stn_chain_config_entry *servers;
    size_t server_count;
} stn_chain_config;

/*
 * Loads the ordered Chain server list from config/chains.json.
 *
 * Array order is preserved and defines deterministic server preference.
 * Returns nonzero on success and zero on failure.
 */
int stn_chain_config_load(
    stn_chain_config *config,
    const char *path);

/*
 * Releases all storage owned by the configuration.
 */
void stn_chain_config_close(
    stn_chain_config *config);

#endif
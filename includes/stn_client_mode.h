/* Copyright (c) 2026 STN-Labz. All rights reserved. */
#ifndef STN_CLIENT_MODE_H
#define STN_CLIENT_MODE_H

#include <stdint.h>

#include "stn_service_mode.h"

/*
 * Mining service mode is connection provenance. It is assigned from the
 * listener that accepted the TCP session and is not supplied or changed by
 * the miner protocol.
 */
typedef struct stn_client_mode {
    stn_service_mode service_mode;
    uint16_t service_port;
} stn_client_mode;

void stn_client_mode_init(
    stn_client_mode *client_mode,
    stn_service_mode service_mode);

int stn_client_mode_valid(const stn_client_mode *client_mode);
int stn_client_mode_is_pool(const stn_client_mode *client_mode);
int stn_client_mode_is_solo(const stn_client_mode *client_mode);

#endif

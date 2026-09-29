/* Copyright (c) 2026 STN-Labz. All rights reserved. */
#ifndef STN_MINING_LISTENER_H
#define STN_MINING_LISTENER_H

#include <stdint.h>

#include "stn_service_mode.h"

#ifdef _WIN32
#include <winsock2.h>
typedef SOCKET stn_mining_socket;
#define STN_MINING_INVALID_SOCKET INVALID_SOCKET
#else
typedef int stn_mining_socket;
#define STN_MINING_INVALID_SOCKET (-1)
#endif

typedef struct stn_mining_listener {
    stn_mining_socket socket;
    stn_service_mode mode;
    uint16_t port;
} stn_mining_listener;

void stn_mining_listener_init(
    stn_mining_listener *listener,
    stn_service_mode mode);

int stn_mining_listener_open(stn_mining_listener *listener);
void stn_mining_listener_close(stn_mining_listener *listener);

#endif

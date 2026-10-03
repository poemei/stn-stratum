/* Copyright (c) 2026 STN-Labz. All rights reserved. */
#ifndef STN_RPC_WIN32_H
#define STN_RPC_WIN32_H

#include <stddef.h>
#include <stdint.h>

typedef struct stn_rpc_win32 {
    const char *host;
    uint16_t port;
    uintptr_t socket_handle;
    int winsock_ready;
} stn_rpc_win32;

void stn_rpc_win32_init(
    stn_rpc_win32 *transport,
    const char *host,
    uint16_t port);

void stn_rpc_win32_close(
    stn_rpc_win32 *transport);

int stn_rpc_win32_exchange(
    void *user,
    const uint8_t *request,
    size_t request_length,
    uint8_t *response,
    size_t response_capacity,
    size_t *response_length);

#endif

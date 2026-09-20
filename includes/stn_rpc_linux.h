/* Copyright (c) 2026 STN-Labz. All rights reserved. */
#ifndef STN_RPC_LINUX_H
#define STN_RPC_LINUX_H

#include <stddef.h>
#include <stdint.h>

typedef struct stn_rpc_linux {
    const char *host;
    uint16_t port;
    int socket_fd;
} stn_rpc_linux;

void stn_rpc_linux_init(
    stn_rpc_linux *transport,
    const char *host,
    uint16_t port);

void stn_rpc_linux_close(
    stn_rpc_linux *transport);

int stn_rpc_linux_exchange(
    void *user,
    const uint8_t *request,
    size_t request_length,
    uint8_t *response,
    size_t response_capacity,
    size_t *response_length);

#endif
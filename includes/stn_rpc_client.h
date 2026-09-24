/* Copyright (c) 2026 STN-Labz. All rights reserved. */
#ifndef STN_RPC_CLIENT_H
#define STN_RPC_CLIENT_H

#include <stddef.h>
#include <stdint.h>

#define STN_RPC_CLIENT_HEADER_SIZE 24u
#define STN_RPC_CLIENT_VERSION 2u
#define STN_RPC_CLIENT_MINER_IDENTITY_SIZE 69u
#define STN_RPC_CLIENT_MAX_PAYLOAD 1052017u
#define STN_RPC_CLIENT_MAX_FRAME (STN_RPC_CLIENT_HEADER_SIZE + STN_RPC_CLIENT_MAX_PAYLOAD)

#define STN_RPC_CLIENT_MINING_TEMPLATE 0x2002u
#define STN_RPC_CLIENT_INFO 0x0001u
#define STN_RPC_CLIENT_SUBMIT_WORK     0x2003u
#define STN_RPC_CLIENT_SUBMIT_SHARE    0x2004u

typedef enum stn_rpc_client_code {
    STN_RPC_CLIENT_OK = 0,
    STN_RPC_CLIENT_INVALID,
    STN_RPC_CLIENT_VERSION_ERROR,
    STN_RPC_CLIENT_METHOD,
    STN_RPC_CLIENT_FORBIDDEN,
    STN_RPC_CLIENT_UNAVAILABLE,
    STN_RPC_CLIENT_NOT_FOUND,
    STN_RPC_CLIENT_REJECTED,
    STN_RPC_CLIENT_PROVIDER,
    STN_RPC_CLIENT_CAPACITY,
    STN_RPC_CLIENT_STALE,
    STN_RPC_CLIENT_TRANSPORT
} stn_rpc_client_code;

typedef int (*stn_rpc_client_exchange_fn)(
    void *user,
    const uint8_t *request,
    size_t request_length,
    uint8_t *response,
    size_t response_capacity,
    size_t *response_length);

typedef struct stn_rpc_client {
    void *transport_user;
    stn_rpc_client_exchange_fn exchange;
    uint64_t next_request_id;
} stn_rpc_client;

void stn_rpc_client_init(
    stn_rpc_client *client,
    void *transport_user,
    stn_rpc_client_exchange_fn exchange);

/* Exact 184-byte STNC v2 INFO payload, including 40-byte big-endian work. */
stn_rpc_client_code stn_rpc_client_info(stn_rpc_client *client,
    uint8_t *payload,size_t capacity,size_t *written);

stn_rpc_client_code stn_rpc_client_mining_template(
    stn_rpc_client *client,
    uint8_t *payload,
    size_t capacity,
    size_t *written);

stn_rpc_client_code stn_rpc_client_submit_work(
    stn_rpc_client *client,
    const uint8_t *payload,
    size_t length,
    uint8_t *response_payload,
    size_t response_capacity,
    size_t *written);

stn_rpc_client_code stn_rpc_client_submit_share(
    stn_rpc_client *client,
    const uint8_t *payload,
    size_t length,
    uint8_t share_id[32]);

#endif

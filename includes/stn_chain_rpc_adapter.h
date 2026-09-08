/* Copyright (c) 2026 STN-Labz. All rights reserved. */
#ifndef STN_CHAIN_RPC_ADAPTER_H
#define STN_CHAIN_RPC_ADAPTER_H

#include "stn_rpc_client.h"
#include "stn_stratum.h"

#define STN_CHAIN_RPC_DEFAULT_HOST "127.0.0.1"
#define STN_CHAIN_RPC_DEFAULT_PORT 18473u

typedef struct stn_chain_rpc_adapter {
    stn_rpc_client *client;
} stn_chain_rpc_adapter;

void stn_chain_rpc_adapter_init(
    stn_chain_rpc_adapter *adapter,
    stn_rpc_client *client);

void stn_chain_rpc_adapter_bind(
    stn_chain_rpc_adapter *adapter,
    stn_stratum_chain *chain);

#endif

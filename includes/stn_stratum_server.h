/* Copyright (c) 2026 STN-Labz. All rights reserved. */
#ifndef STN_STRATUM_SERVER_H
#define STN_STRATUM_SERVER_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "stn_rpc_client.h"
#include "stn_rpc_win32.h"
#include "stn_miner_protocol.h"

#define STN_STRATUM_DEFAULT_PORT 18475u
#define STN_STRATUM_CHAIN_HOST "127.0.0.1"
#define STN_STRATUM_CHAIN_PORT 18473u
#define STN_STRATUM_POLL_MS 250u
#define STN_STRATUM_CHAIN_POLL_TICKS 4u
#define STN_STRATUM_LOG_PATH "logs\\stn-stratum.log"
#define STN_STRATUM_HEARTBEAT_CHAIN_POLLS 30u

typedef struct stn_stratum_client_session stn_stratum_client_session;

typedef struct stn_stratum_server {
    stn_rpc_win32 chain_transport;
    stn_rpc_client chain_rpc;

    uint8_t active_base[32];
    uint8_t active_job_id[32];
    uint8_t active_template[STN_RPC_CLIENT_MAX_PAYLOAD];
    size_t active_template_length;
    int have_work;

    uintptr_t listen_socket;
    stn_stratum_client_session *clients;
    size_t client_count;

    int running;
    int chain_available;
    int winsock_ready;

    FILE *log_file;
    uint64_t loop_count;
    uint64_t chain_poll_count;
} stn_stratum_server;

void stn_stratum_server_init(stn_stratum_server *server);
int stn_stratum_server_run(stn_stratum_server *server);
void stn_stratum_server_stop(stn_stratum_server *server);
void stn_stratum_server_close(stn_stratum_server *server);

#endif

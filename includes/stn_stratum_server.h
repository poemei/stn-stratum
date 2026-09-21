#ifndef STN_STRATUM_SERVER_H
#define STN_STRATUM_SERVER_H

#include <stddef.h>
#include <stdint.h>
#include <time.h>

#include "stn_chain_config.h"
#include "stn_chain_rpc_adapter.h"

#ifdef _WIN32
#include <winsock2.h>
#include "stn_rpc_win32.h"
#else
#include "stn_rpc_linux.h"
#endif

#define STN_STRATUM_PORT 18475u
#define STN_STRATUM_TELEMETRY_PORT 18476u

typedef struct stn_stratum_client_session stn_stratum_client_session;

typedef struct stn_stratum_server {
#ifdef _WIN32
    SOCKET listen_socket;
    SOCKET telemetry_socket;
    stn_rpc_win32 chain_transport;
#else
    int listen_socket;
    int telemetry_socket;
    stn_rpc_linux chain_transport;
#endif

    stn_chain_config chain_config;
    size_t chain_index;

    stn_chain_rpc_adapter chain_adapter;

    stn_stratum_client_session *clients;

    uint8_t *work;
    size_t work_length;

    int have_work;
    int running;

    time_t started_at;
} stn_stratum_server;

void stn_stratum_server_init(
    stn_stratum_server *server
);

void stn_stratum_server_close(
    stn_stratum_server *server
);

void stn_stratum_server_stop(
    stn_stratum_server *server
);

int stn_stratum_server_run(
    stn_stratum_server *server
);

#endif
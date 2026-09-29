/* Copyright (c) 2026 STN-Labz. All rights reserved. */
#ifndef STN_STRATUM_SERVER_H
#define STN_STRATUM_SERVER_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

#include "stn_rpc_client.h"
#include "stn_miner_protocol.h"
#include "stn_chain_config.h"

#ifdef _WIN32
#include "stn_rpc_win32.h"
#elif defined(__linux__)
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include "stn_rpc_linux.h"
#else
#error Unsupported STN-Stratum platform
#endif

#define STN_STRATUM_DEFAULT_PORT 18475u
#define STN_STRATUM_TELEMETRY_PORT 18476u

#define STN_STRATUM_POLL_MS 250u
#define STN_STRATUM_CHAIN_POLL_TICKS 4u
#define STN_STRATUM_HEARTBEAT_CHAIN_POLLS 30u

#ifdef _WIN32
#define STN_STRATUM_CHAIN_CONFIG "config/chains.json"
#define STN_STRATUM_LOG_PATH "logs\\stn-stratum.log"
#elif defined(__linux__)
#define STN_STRATUM_CHAIN_CONFIG "/etc/stn-stratum/config.json"
#define STN_STRATUM_LOG_PATH "/var/log/stratum/stratum.log"
#endif

/*
 * Linux server receive operations must never obtain an unbounded blocking
 * position in the primary Stratum service loop. Miner sockets are already
 * configured nonblocking; this wrapper also bounds accepted telemetry
 * sockets before telemetry_service() has an opportunity to wait on them.
 *
 * A telemetry connection with no readable request bytes therefore produces
 * EAGAIN/EWOULDBLOCK and is closed by the existing telemetry path instead of
 * blocking miner servicing, Chain polling, work refresh, or later clients.
 */
#ifdef __linux__
static inline ssize_t stn_stratum_recv_nonblocking(
    int socket_fd,
    void *buffer,
    size_t length,
    int flags)
{
    return recv(
        socket_fd,
        buffer,
        length,
        flags|MSG_DONTWAIT);
}
#define recv(socket_fd,buffer,length,flags) \
    stn_stratum_recv_nonblocking((socket_fd),(buffer),(length),(flags))
#endif

typedef struct stn_stratum_client_session stn_stratum_client_session;

typedef struct stn_stratum_server {
    stn_chain_config chain_config;
    size_t active_chain_server;

#ifdef _WIN32
    stn_rpc_win32 chain_transport;
#elif defined(__linux__)
    stn_rpc_linux chain_transport;
#endif

    stn_rpc_client chain_rpc;

    uint8_t active_base[32];
    uint8_t active_job_id[32];
    uint8_t active_template[STN_RPC_CLIENT_MAX_PAYLOAD];
    size_t active_template_length;
    int have_work;

    uintptr_t listen_socket;
    uintptr_t telemetry_socket;

    stn_stratum_client_session *clients;
    size_t client_count;

    int running;
    int chain_available;

#ifdef _WIN32
    int winsock_ready;
#endif

    FILE *log_file;

    uint64_t loop_count;
    uint64_t chain_poll_count;

    time_t started_at;
} stn_stratum_server;

void stn_stratum_server_init(stn_stratum_server *server);
int stn_stratum_server_run(stn_stratum_server *server);
void stn_stratum_server_stop(stn_stratum_server *server);
void stn_stratum_server_close(stn_stratum_server *server);

#endif

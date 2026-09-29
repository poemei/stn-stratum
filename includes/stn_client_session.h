/* Copyright (c) 2026 STN-Labz. All rights reserved. */
#ifndef STN_CLIENT_SESSION_H
#define STN_CLIENT_SESSION_H

#include <stddef.h>
#include <stdint.h>

#include "stn_miner_protocol.h"
#include "stn_client_mode.h"

#ifdef _WIN32
#include <winsock2.h>
typedef SOCKET stn_client_socket;
#define STN_CLIENT_INVALID_SOCKET INVALID_SOCKET
#else
typedef int stn_client_socket;
#define STN_CLIENT_INVALID_SOCKET (-1)
#endif

typedef struct stn_client_session {
    stn_client_socket socket;
    uint8_t rx[STN_MINER_ADDRESS_FRAME_SIZE];
    size_t rx_used;
    uint8_t sent_job_id[32];
    uint64_t hashrate;
    char address[STN_MINER_ADDRESS_SIZE];
    stn_client_mode mode;
    int address_registered;
    int has_job;
    struct stn_client_session *next;
} stn_client_session;

/* [AI:GPT-5.6 Sol | 2026-09-29 00:00:00 UTC] */
void stn_client_session_init(
    stn_client_session *session,
    stn_client_socket socket_fd,
    stn_client_mode mode);
void stn_client_session_clear_job(stn_client_session *session);
uint64_t stn_client_session_total_hashrate(const stn_client_session *sessions);
size_t stn_client_session_count(const stn_client_session *sessions);
/* [End AI:GPT-5.6 Sol] */

#endif

/* Copyright (c) 2026 STN-Labz. All rights reserved. */
#ifndef STN_TELEMETRY_H
#define STN_TELEMETRY_H

#include <stddef.h>
#include <stdint.h>
#include <time.h>

#define STN_TELEMETRY_HTTP_REQUEST_SIZE 1024u
#define STN_TELEMETRY_HTTP_RESPONSE_SIZE 4096u
#define STN_TELEMETRY_CLIENT_TIMEOUT_SECONDS 2u

#ifdef _WIN32
#include <winsock2.h>
typedef SOCKET stn_telemetry_socket;
#define STN_TELEMETRY_INVALID_SOCKET INVALID_SOCKET
#else
typedef int stn_telemetry_socket;
#define STN_TELEMETRY_INVALID_SOCKET (-1)
#endif

typedef struct stn_telemetry_status {
    int chain_connected;
    const char *chain_host;
    uint16_t chain_port;
    int work_available;
    size_t miners;
    uint64_t hashrate;
    const char *job_id;
    const char *base_id;
    const char *target;
    uint64_t uptime_seconds;
} stn_telemetry_status;

typedef struct stn_telemetry_client {
    stn_telemetry_socket socket;
    char request[STN_TELEMETRY_HTTP_REQUEST_SIZE];
    size_t request_length;
    char response[STN_TELEMETRY_HTTP_RESPONSE_SIZE];
    size_t response_length;
    size_t response_sent;
    time_t connected_at;
    struct stn_telemetry_client *next;
} stn_telemetry_client;

typedef struct stn_telemetry_service {
    stn_telemetry_socket listen_socket;
    stn_telemetry_client *clients;
    size_t client_count;
    uint16_t port;
} stn_telemetry_service;

/* [AI:GPT-5.6 Sol | 2026-09-29 00:00:00 UTC] */
int stn_telemetry_request_valid(const char *request, size_t request_length);
int stn_telemetry_status_json(
    const stn_telemetry_status *status,
    char *output,
    size_t output_size);
int stn_telemetry_http_response(
    const stn_telemetry_status *status,
    char *output,
    size_t output_size);

void stn_telemetry_service_init(
    stn_telemetry_service *service,
    uint16_t port);
int stn_telemetry_service_open(stn_telemetry_service *service);
void stn_telemetry_service_tick(
    stn_telemetry_service *service,
    const stn_telemetry_status *status,
    time_t now);
void stn_telemetry_service_close(stn_telemetry_service *service);
/* [End AI:GPT-5.6 Sol] */

#endif

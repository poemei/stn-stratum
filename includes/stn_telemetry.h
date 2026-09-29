/* Copyright (c) 2026 STN-Labz. All rights reserved. */
#ifndef STN_TELEMETRY_H
#define STN_TELEMETRY_H

#include <stddef.h>
#include <stdint.h>

#define STN_TELEMETRY_HTTP_REQUEST_SIZE 1024u

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
/* [End AI:GPT-5.6 Sol] */

#endif

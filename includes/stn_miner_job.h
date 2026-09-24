/* Copyright (c) 2026 STN-Labz. All rights reserved. */
#ifndef STN_MINER_JOB_H
#define STN_MINER_JOB_H
#include <stddef.h>
#include <stdint.h>
#include "stn_miner_protocol.h"

/* Build the exact 116-byte Miner JOB header from one Chain mining-template
 * response. The template is the STNC 68-byte mining prefix followed by the
 * canonical block. No socket or platform behavior is part of this codec. */
int stn_miner_job_build(
    const uint8_t *template_bytes,
    size_t template_length,
    uint8_t header[STN_MINER_JOB_HEADER_SIZE],
    uint32_t *block_length);
#endif

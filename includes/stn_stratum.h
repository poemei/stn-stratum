/* Copyright (c) 2026 STN-Labz. All rights reserved. */
#ifndef STN_STRATUM_H
#define STN_STRATUM_H

#include <stddef.h>
#include <stdint.h>

#define STN_STRATUM_PROTOCOL_VERSION 1u
#define STN_STRATUM_DUPLICATE_CAPACITY 64u
#define STN_STRATUM_MINER_IDENTITY_SIZE 69u

/*
 * Stratum owns its protocol buffer capacity. The chain adapter remains
 * authoritative for whether a supplied block/template is valid.
 */
#ifndef STN_STRATUM_BLOCK_CAPACITY
#define STN_STRATUM_BLOCK_CAPACITY 1048576u
#endif

typedef enum stn_stratum_status {
    STN_STRATUM_OK=0,
    STN_STRATUM_ARGUMENT,
    STN_STRATUM_STRUCTURE,
    STN_STRATUM_PROFILE,
    STN_STRATUM_BASE,
    STN_STRATUM_PROVIDER,
    STN_STRATUM_NO_JOB,
    STN_STRATUM_STALE,
    STN_STRATUM_DUPLICATE,
    STN_STRATUM_REJECTED,
    STN_STRATUM_CAPACITY
} stn_stratum_status;

typedef struct stn_stratum_job {
    uint8_t base_tip[32];
    uint8_t id[32];
    uint8_t target[32];
    uint8_t block[STN_STRATUM_BLOCK_CAPACITY];
    size_t block_length;
    uint64_t height;
    int active;
} stn_stratum_job;

/*
 * [AI:GPT-5.6 Sol | 2026-09-07 21:32:50 UTC]
 *
 * Platform-agnostic STN-Chain boundary.
 *
 * The adapter, not Stratum, owns STN-Chain serialization and consensus
 * details. This keeps the Stratum core independent of chain-internal headers
 * such as stn_block.h.
 */
typedef stn_stratum_status (*stn_stratum_tip_fn)(
    void *user,
    uint8_t tip[32]);

typedef stn_stratum_status (*stn_stratum_template_fn)(
    void *user,
    uint8_t base_tip[32],
    uint8_t job_id[32],
    uint8_t target[32],
    uint64_t *height,
    uint8_t *block,
    size_t capacity,
    size_t *written);

typedef stn_stratum_status (*stn_stratum_submit_fn)(
    void *user,
    const uint8_t base_tip[32],
    const uint8_t job_id[32],
    const uint8_t *block,
    size_t block_length,
    const uint8_t miner_identity[STN_STRATUM_MINER_IDENTITY_SIZE],
    uint64_t nonce);

typedef struct stn_stratum_chain {
    void *user;
    stn_stratum_tip_fn tip;
    stn_stratum_template_fn template_get;
    stn_stratum_submit_fn submit;
} stn_stratum_chain;
/* [End AI:GPT-5.6 Sol] */

typedef struct stn_stratum_session {
    uint64_t session_id;
    uint64_t submitted[STN_STRATUM_DUPLICATE_CAPACITY];
    size_t submitted_count;
    uint8_t miner_identity[STN_STRATUM_MINER_IDENTITY_SIZE];
    int identity_registered;
    int subscribed;
} stn_stratum_session;

void stn_stratum_session_init(
    stn_stratum_session *session,
    uint64_t session_id);

stn_stratum_status stn_stratum_subscribe(
    stn_stratum_session *session,
    uint16_t version);

stn_stratum_status stn_stratum_register_identity(
    stn_stratum_session *session,
    const uint8_t miner_identity[STN_STRATUM_MINER_IDENTITY_SIZE]);

stn_stratum_status stn_stratum_job_set(
    stn_stratum_job *job,
    const uint8_t base_tip[32],
    const uint8_t job_id[32],
    const uint8_t target[32],
    uint64_t height,
    const uint8_t *block,
    size_t block_length);

stn_stratum_status stn_stratum_job_refresh(
    stn_stratum_job *job,
    const stn_stratum_chain *chain);

void stn_stratum_job_invalidate(
    stn_stratum_job *job);

stn_stratum_status stn_stratum_submit(
    stn_stratum_session *session,
    const stn_stratum_job *job,
    uint64_t nonce,
    const stn_stratum_chain *chain);

#endif

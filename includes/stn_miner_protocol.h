/* Copyright (c) 2026 STN-Labz. All rights reserved. */
/*
 * Temporary STN-Stratum development miner protocol.
 * This exists only to qualify miner <-> Stratum <-> Chain plumbing.
 */
#ifndef STN_MINER_PROTOCOL_H
#define STN_MINER_PROTOCOL_H

#include <stdint.h>

#define STN_MINER_MAGIC "STNM"
#define STN_MINER_VERSION 1u

#define STN_MINER_JOB           1u
#define STN_MINER_SUBMIT        2u
#define STN_MINER_RESULT        3u
#define STN_MINER_HASH_PROGRESS 4u

#define STN_MINER_JOB_HEADER_SIZE    84u
#define STN_MINER_SUBMIT_SIZE        48u
#define STN_MINER_RESULT_SIZE        12u
#define STN_MINER_HASH_PROGRESS_SIZE 56u

#define STN_MINER_CHAIN_HEADER_SIZE 168u
#define STN_MINER_CHAIN_TARGET_OFFSET 120u
#define STN_MINER_CHAIN_NONCE_OFFSET 152u

#define STN_MINER_RESULT_ACCEPTED 0u
#define STN_MINER_RESULT_REJECTED 1u
#define STN_MINER_RESULT_STALE 2u
#define STN_MINER_RESULT_PROVIDER 3u
#define STN_MINER_RESULT_PROTOCOL 4u

#endif
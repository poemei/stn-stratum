/* Copyright (c) 2026 STN-Labz. All rights reserved. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "stn_client_session.h"

static unsigned checks;
static unsigned failures;

#define CHECK(expression) do { \
    ++checks; \
    if(!(expression)){ \
        ++failures; \
        printf("FAIL line %d: %s\n",__LINE__,#expression); \
    } \
} while(0)

/* [AI:GPT-5.6 Sol | 2026-09-29 00:00:00 UTC] */
int main(void)
{
    stn_client_session pool;
    stn_client_session solo;
    stn_client_mode pool_mode;
    stn_client_mode solo_mode;

    stn_client_mode_init(&pool_mode,STN_SERVICE_MODE_POOL);
    stn_client_session_init(&pool,10,pool_mode);
    CHECK(pool.socket==10);
    CHECK(stn_client_mode_is_pool(&pool.mode));
    CHECK(pool.mode.service_mode==STN_SERVICE_MODE_POOL);
    CHECK(pool.mode.service_port==stn_service_mode_port(STN_SERVICE_MODE_POOL));
    CHECK(pool.rx_used==0u);
    CHECK(pool.hashrate==0u);
    CHECK(pool.address_registered==0);
    CHECK(pool.has_job==0);
    CHECK(pool.next==NULL);

    stn_client_mode_init(&solo_mode,STN_SERVICE_MODE_SOLO);
    stn_client_session_init(&solo,11,solo_mode);
    CHECK(solo.socket==11);
    CHECK(stn_client_mode_is_solo(&solo.mode));
    CHECK(solo.mode.service_mode==STN_SERVICE_MODE_SOLO);
    CHECK(solo.mode.service_port==stn_service_mode_port(STN_SERVICE_MODE_SOLO));

    pool.hashrate=100u;
    solo.hashrate=250u;
    pool.next=&solo;
    CHECK(stn_client_session_count(&pool)==2u);
    CHECK(stn_client_session_total_hashrate(&pool)==350u);

    memset(pool.sent_job_id,0xa5,sizeof(pool.sent_job_id));
    pool.has_job=1;
    stn_client_session_clear_job(&pool);
    CHECK(pool.hashrate==0u);
    CHECK(pool.has_job==0);
    CHECK(pool.sent_job_id[0]==0u);
    CHECK(pool.sent_job_id[31]==0u);

    pool.hashrate=UINT64_MAX;
    solo.hashrate=1u;
    CHECK(stn_client_session_total_hashrate(&pool)==UINT64_MAX);

    CHECK(stn_client_session_count(NULL)==0u);
    CHECK(stn_client_session_total_hashrate(NULL)==0u);

    printf("Client session: %u checks, %u failures.\n",checks,failures);
    return failures!=0u ? 1 : 0;
}
/* [End AI:GPT-5.6 Sol] */

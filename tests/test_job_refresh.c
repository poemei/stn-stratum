/* Copyright (c) 2026 STN-Labz. All rights reserved. */
#include <stdio.h>
#include <string.h>

#include "stn_stratum.h"

static unsigned template_calls;
static unsigned tip_calls;

static stn_stratum_status fixture_tip(void *user,uint8_t tip[32])
{
    (void)user;
    ++tip_calls;
    memset(tip,0x22,32u);
    return STN_STRATUM_OK;
}

static stn_stratum_status fixture_template(
    void *user,
    uint8_t base_tip[32],
    uint8_t job_id[32],
    uint8_t target[32],
    uint64_t *height,
    uint8_t *block,
    size_t capacity,
    size_t *written)
{
    (void)user;
    ++template_calls;
    if(capacity<168u)return STN_STRATUM_CAPACITY;
    memset(base_tip,0x11,32u);
    memset(job_id,0x33,32u);
    memset(target,0x44,32u);
    memset(block,0,168u);
    *height=452u;
    *written=168u;
    return STN_STRATUM_OK;
}

int main(void)
{
    stn_stratum_job job;
    stn_stratum_chain chain;
    stn_stratum_status status;

    memset(&job,0,sizeof(job));
    memset(&chain,0,sizeof(chain));
    chain.tip=fixture_tip;
    chain.template_get=fixture_template;

    status=stn_stratum_job_refresh(&job,&chain);
    if(status!=STN_STRATUM_OK)return 1;
    if(template_calls!=1u)return 1;
    if(tip_calls!=0u)return 1;
    if(!job.active||job.height!=452u||job.block_length!=168u)return 1;
    if(job.base_tip[0]!=0x11u||job.id[0]!=0x33u||job.target[0]!=0x44u)return 1;

    puts("Job refresh: one canonical template request, no redundant tip request. PASS");
    return 0;
}

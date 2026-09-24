/* Copyright (c) 2026 STN-Labz. All rights reserved. */
#include "stn_stratum.h"

#include <string.h>

void stn_stratum_session_init(stn_stratum_session *s,uint64_t id)
{
    if(s!=NULL)
    {
        memset(s,0,sizeof(*s));
        s->session_id=id;
    }
}

stn_stratum_status stn_stratum_subscribe(stn_stratum_session *s,uint16_t version)
{
    if(s==NULL)
    {
        return STN_STRATUM_ARGUMENT;
    }
    if(version!=STN_STRATUM_PROTOCOL_VERSION)
    {
        return STN_STRATUM_PROFILE;
    }

    s->subscribed=1;
    return STN_STRATUM_OK;
}

stn_stratum_status stn_stratum_register_identity(
    stn_stratum_session *s,
    const uint8_t miner_identity[STN_STRATUM_MINER_IDENTITY_SIZE])
{
    if(s==NULL||miner_identity==NULL){return STN_STRATUM_ARGUMENT;}
    if(memcmp(miner_identity,"stn0_",5u)!=0 ||
       miner_identity[STN_STRATUM_MINER_IDENTITY_SIZE-1u]!='\0'){
        return STN_STRATUM_STRUCTURE;
    }
    memcpy(s->miner_identity,miner_identity,STN_STRATUM_MINER_IDENTITY_SIZE);
    s->identity_registered=1;
    return STN_STRATUM_OK;
}

/*
 * [AI:GPT-5.6 Sol | 2026-09-07 21:32:50 UTC]
 */
stn_stratum_status stn_stratum_job_set(
    stn_stratum_job *out,
    const uint8_t base_tip[32],
    const uint8_t job_id[32],
    const uint8_t target[32],
    uint64_t height,
    const uint8_t *block,
    size_t block_length)
{
    if(out==NULL||base_tip==NULL||job_id==NULL||target==NULL||block==NULL)
    {
        return STN_STRATUM_ARGUMENT;
    }
    if(block_length==0u)
    {
        return STN_STRATUM_STRUCTURE;
    }
    if(block_length>STN_STRATUM_BLOCK_CAPACITY)
    {
        return STN_STRATUM_CAPACITY;
    }

    memset(out,0,sizeof(*out));
    memcpy(out->base_tip,base_tip,32);
    memcpy(out->id,job_id,32);
    memcpy(out->target,target,32);
    memcpy(out->block,block,block_length);
    out->block_length=block_length;
    out->height=height;
    out->active=1;

    return STN_STRATUM_OK;
}

stn_stratum_status stn_stratum_job_refresh(
    stn_stratum_job *job,
    const stn_stratum_chain *chain)
{
    uint8_t live_tip[32];
    uint8_t base_tip[32];
    uint8_t job_id[32];
    uint8_t target[32];
    uint8_t block[STN_STRATUM_BLOCK_CAPACITY];
    uint64_t height;
    size_t written;
    stn_stratum_status status;

    if(job==NULL||chain==NULL||chain->tip==NULL||
       chain->template_get==NULL)
    {
        return STN_STRATUM_ARGUMENT;
    }

    status=chain->tip(chain->user,live_tip);
    if(status!=STN_STRATUM_OK)
    {
        return STN_STRATUM_PROVIDER;
    }

    written=0u;
    height=0u;
    status=chain->template_get(
        chain->user,
        base_tip,
        job_id,
        target,
        &height,
        block,
        sizeof(block),
        &written);

    if(status!=STN_STRATUM_OK)
    {
        return status==STN_STRATUM_CAPACITY
            ? STN_STRATUM_CAPACITY
            : STN_STRATUM_PROVIDER;
    }

    if(memcmp(base_tip,live_tip,32)!=0)
    {
        return STN_STRATUM_STALE;
    }

    return stn_stratum_job_set(
        job,
        base_tip,
        job_id,
        target,
        height,
        block,
        written);
}
/* [End AI:GPT-5.6 Sol] */

void stn_stratum_job_invalidate(stn_stratum_job *job)
{
    if(job!=NULL)
    {
        job->active=0;
    }
}

/*
 * [AI:GPT-5.6 Sol | 2026-09-07 21:32:50 UTC]
 */
stn_stratum_status stn_stratum_submit(
    stn_stratum_session *s,
    const stn_stratum_job *j,
    uint64_t nonce,
    const stn_stratum_chain *chain)
{
    uint8_t tip[32];
    size_t i;
    stn_stratum_status status;

    if(s==NULL||j==NULL||chain==NULL||
       chain->tip==NULL||chain->submit==NULL)
    {
        return STN_STRATUM_ARGUMENT;
    }

    if(!s->subscribed||!j->active)
    {
        return STN_STRATUM_NO_JOB;
    }
    if(!s->identity_registered)
    {
        return STN_STRATUM_STRUCTURE;
    }

    status=chain->tip(chain->user,tip);
    if(status!=STN_STRATUM_OK)
    {
        return STN_STRATUM_PROVIDER;
    }

    if(memcmp(j->base_tip,tip,32)!=0)
    {
        return STN_STRATUM_STALE;
    }

    for(i=0;i<s->submitted_count;i++)
    {
        if(s->submitted[i]==nonce)
        {
            return STN_STRATUM_DUPLICATE;
        }
    }

    if(s->submitted_count==STN_STRATUM_DUPLICATE_CAPACITY)
    {
        return STN_STRATUM_CAPACITY;
    }

    /*
     * Record before provider submission so identical work cannot be retried
     * through this session merely because the provider rejected it.
     */
    s->submitted[s->submitted_count++]=nonce;

    status=chain->submit(
        chain->user,
        j->base_tip,
        j->id,
        j->block,
        j->block_length,
        s->miner_identity,
        nonce);

    if(status==STN_STRATUM_OK||
       status==STN_STRATUM_STALE||
       status==STN_STRATUM_REJECTED)
    {
        return status;
    }

    return STN_STRATUM_PROVIDER;
}
/* [End AI:GPT-5.6 Sol] */

/* Copyright (c) 2026 STN-Labz. All rights reserved. */
#include "stn_miner_job.h"
#include <string.h>

static void write32be(uint8_t out[4],uint32_t value)
{
    out[0]=(uint8_t)(value>>24);out[1]=(uint8_t)(value>>16);
    out[2]=(uint8_t)(value>>8);out[3]=(uint8_t)value;
}
static int share_target_from_chain(const uint8_t chain[32],uint8_t share[32])
{
    uint8_t scaled[33]={0};
    unsigned carry=0u;
    size_t i;

    if(chain==NULL||share==NULL)return 0;

    for(i=32u;i!=0u;--i){
        unsigned value=(unsigned)chain[i-1u]*STN_MINER_SHARE_FACTOR+carry;
        scaled[i]=(uint8_t)(value&0xffu);
        carry=value>>8;
    }
    scaled[0]=(uint8_t)carry;

    /*
     * Match the Chain economic rule exactly:
     *     min(MAX_TARGET, chain_target * STN_MINER_SHARE_FACTOR)
     * MAX_TARGET is 2^255-1.  A carry into scaled[0], or bit 255 becoming
     * set in the 256-bit product, means the product exceeds MAX_TARGET.
     */
    if(scaled[0]!=0u||scaled[1]>=0x80u){
        memset(share,0xff,32u);
        share[0]=0x7fu;
    }else{
        memcpy(share,scaled+1u,32u);
    }

    return 1;
}
int stn_miner_job_build(const uint8_t *t,size_t n,uint8_t h[STN_MINER_JOB_HEADER_SIZE],uint32_t *block_length)
{
    size_t block;
    if(t==NULL||h==NULL||block_length==NULL||n<68u)return 0;
    block=n-68u;
    if(block<STN_MINER_CHAIN_HEADER_SIZE||block>UINT32_MAX)return 0;
    memset(h,0,STN_MINER_JOB_HEADER_SIZE);
    memcpy(h,STN_MINER_MAGIC,4u);h[4]=STN_MINER_VERSION;h[5]=STN_MINER_JOB;
    memcpy(h+8u,t+32u,32u);
    if(!share_target_from_chain(t+68u+STN_MINER_CHAIN_TARGET_OFFSET,h+STN_MINER_JOB_SHARE_TARGET_OFFSET))return 0;
    memcpy(h+STN_MINER_JOB_CHAIN_TARGET_OFFSET,t+68u+STN_MINER_CHAIN_TARGET_OFFSET,32u);
    write32be(h+STN_MINER_JOB_BLOCK_LENGTH_OFFSET,(uint32_t)block);
    memcpy(h+STN_MINER_JOB_INITIAL_NONCE_OFFSET,t+68u+STN_MINER_CHAIN_NONCE_OFFSET,8u);
    *block_length=(uint32_t)block;
    return 1;
}

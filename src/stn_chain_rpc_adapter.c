/* Copyright (c) 2026 STN-Labz. All rights reserved. */
#include "stn_chain_rpc_adapter.h"

#include <string.h>

#define STN_CHAIN_BLOCK_HEADER_SIZE   168u
#define STN_CHAIN_BLOCK_HEIGHT_OFFSET 72u
#define STN_CHAIN_BLOCK_TARGET_OFFSET 120u
#define STN_CHAIN_BLOCK_NONCE_OFFSET  152u

static uint32_t read32be(const uint8_t *p)
{
    return ((uint32_t)p[0]<<24)|
           ((uint32_t)p[1]<<16)|
           ((uint32_t)p[2]<<8)|
           (uint32_t)p[3];
}

static uint64_t read64be(const uint8_t *p)
{
    uint64_t v=0u;
    size_t i;

    for(i=0;i<8u;i++){
        v=(v<<8)|p[i];
    }

    return v;
}

static void write64be(uint8_t *p,uint64_t v)
{
    size_t i;
    for(i=0;i<8;i++){
        p[7u-i]=(uint8_t)(v&0xffu);
        v>>=8;
    }
}

static stn_stratum_status map_rpc(stn_rpc_client_code code)
{
    switch(code){
    case STN_RPC_CLIENT_OK:return STN_STRATUM_OK;
    case STN_RPC_CLIENT_REJECTED:return STN_STRATUM_REJECTED;
    case STN_RPC_CLIENT_STALE:return STN_STRATUM_STALE;
    case STN_RPC_CLIENT_CAPACITY:return STN_STRATUM_CAPACITY;
    default:return STN_STRATUM_PROVIDER;
    }
}

/*
 * We deliberately use MINING_TEMPLATE for the current base tip.
 * The RPC implementation guarantees the first 32 payload bytes are the
 * work base because MINING_TEMPLATE has exactly SUBMIT_WORK payload shape.
 */
static stn_stratum_status rpc_tip(void *user,uint8_t tip[32])
{
    stn_chain_rpc_adapter *a=(stn_chain_rpc_adapter *)user;
    uint8_t payload[STN_RPC_CLIENT_MAX_PAYLOAD];
    size_t written=0u;
    stn_rpc_client_code code;

    if(a==NULL||a->client==NULL||tip==NULL){
        return STN_STRATUM_ARGUMENT;
    }

    code=stn_rpc_client_mining_template(
        a->client,payload,sizeof(payload),&written);

    if(code!=STN_RPC_CLIENT_OK){
        return map_rpc(code);
    }

    if(written<68u+STN_CHAIN_BLOCK_HEADER_SIZE ||
       read32be(payload+64)!=(uint32_t)(written-68u)){
        return STN_STRATUM_STRUCTURE;
    }

    memcpy(tip,payload,32);
    return STN_STRATUM_OK;
}

static stn_stratum_status rpc_template(
    void *user,
    uint8_t base_tip[32],
    uint8_t job_id[32],
    uint8_t target[32],
    uint64_t *height,
    uint8_t *block,
    size_t capacity,
    size_t *written)
{
    stn_chain_rpc_adapter *a=(stn_chain_rpc_adapter *)user;
    uint8_t payload[STN_RPC_CLIENT_MAX_PAYLOAD];
    size_t n=0u;
    uint32_t block_length;
    stn_rpc_client_code code;

    if(written!=NULL){*written=0;}

    if(a==NULL||a->client==NULL||base_tip==NULL||
       job_id==NULL||target==NULL||height==NULL||
       block==NULL||written==NULL)
    {
        return STN_STRATUM_ARGUMENT;
    }

    code=stn_rpc_client_mining_template(
        a->client,payload,sizeof(payload),&n);

    if(code!=STN_RPC_CLIENT_OK){
        return map_rpc(code);
    }

    if(n<68u+STN_CHAIN_BLOCK_HEADER_SIZE){
        return STN_STRATUM_STRUCTURE;
    }

    block_length=read32be(payload+64);

    if((size_t)block_length!=n-68u){
        return STN_STRATUM_STRUCTURE;
    }

    if(block_length<STN_CHAIN_BLOCK_HEADER_SIZE){
        return STN_STRATUM_STRUCTURE;
    }

    if((size_t)block_length>capacity){
        return STN_STRATUM_CAPACITY;
    }

    memcpy(base_tip,payload,32);
    memcpy(job_id,payload+32,32);
    memcpy(block,payload+68,block_length);

    /*
     * Mining metadata is taken directly from the canonical encoded block
     * supplied by STN Chain. Stratum does not calculate or reinterpret
     * consensus state.
     */
    memcpy(target,
        payload+68u+STN_CHAIN_BLOCK_TARGET_OFFSET,
        32u);

    *height=read64be(
        payload+68u+STN_CHAIN_BLOCK_HEIGHT_OFFSET);

    *written=block_length;
    return STN_STRATUM_OK;
}

static stn_stratum_status rpc_submit(
    void *user,
    const uint8_t base_tip[32],
    const uint8_t job_id[32],
    const uint8_t *block,
    size_t block_length,
    uint64_t nonce)
{
    stn_chain_rpc_adapter *a=(stn_chain_rpc_adapter *)user;
    uint8_t payload[STN_RPC_CLIENT_MAX_PAYLOAD];
    uint8_t response[80];
    size_t n=0u;
    stn_rpc_client_code code;

    if(a==NULL||a->client==NULL||base_tip==NULL||
       job_id==NULL||block==NULL||
       block_length<STN_CHAIN_BLOCK_HEADER_SIZE||
       block_length>STN_RPC_CLIENT_MAX_PAYLOAD-68u)
    {
        return STN_STRATUM_ARGUMENT;
    }

    memcpy(payload,base_tip,32);
    memcpy(payload+32,job_id,32);

    payload[64]=(uint8_t)(block_length>>24);
    payload[65]=(uint8_t)(block_length>>16);
    payload[66]=(uint8_t)(block_length>>8);
    payload[67]=(uint8_t)block_length;

    memcpy(payload+68,block,block_length);

    /*
     * Only the canonical nonce field is changed. All other work bytes remain
     * exactly as supplied by STN Chain.
     */
    write64be(
        payload+68u+STN_CHAIN_BLOCK_NONCE_OFFSET,
        nonce);

    code=stn_rpc_client_submit_work(
        a->client,
        payload,
        68u+block_length,
        response,
        sizeof(response),
        &n);

    if(code!=STN_RPC_CLIENT_OK){
        return map_rpc(code);
    }

    if(n!=80u){
        return STN_STRATUM_STRUCTURE;
    }

    return STN_STRATUM_OK;
}

void stn_chain_rpc_adapter_init(
    stn_chain_rpc_adapter *adapter,
    stn_rpc_client *client)
{
    if(adapter!=NULL){
        adapter->client=client;
    }
}

void stn_chain_rpc_adapter_bind(
    stn_chain_rpc_adapter *adapter,
    stn_stratum_chain *chain)
{
    if(chain!=NULL){
        chain->user=adapter;
        chain->tip=rpc_tip;
        chain->template_get=rpc_template;
        chain->submit=rpc_submit;
    }
}
/* Copyright (c) 2026 STN-Labz. All rights reserved. */
#include "stn_rpc_client.h"

#include <stdlib.h>
#include <string.h>

#define STN_RPC_CLIENT_BLOCK_HEADER_SIZE 168u
#define STN_RPC_CLIENT_MINING_PREFIX_SIZE 68u

static uint64_t read_be(const uint8_t *p,size_t n)
{
    uint64_t v=0;
    size_t i;
    for(i=0;i<n;i++){v=(v<<8)|p[i];}
    return v;
}

static void write_be(uint8_t *p,size_t n,uint64_t v)
{
    size_t i;
    for(i=0;i<n;i++){p[n-1u-i]=(uint8_t)(v&0xffu);v>>=8;}
}

static stn_rpc_client_code map_code(uint16_t code)
{
    switch(code){
    case 0:return STN_RPC_CLIENT_OK;
    case 1:return STN_RPC_CLIENT_INVALID;
    case 2:return STN_RPC_CLIENT_VERSION_ERROR;
    case 3:return STN_RPC_CLIENT_METHOD;
    case 4:return STN_RPC_CLIENT_FORBIDDEN;
    case 5:return STN_RPC_CLIENT_UNAVAILABLE;
    case 6:return STN_RPC_CLIENT_NOT_FOUND;
    case 7:return STN_RPC_CLIENT_REJECTED;
    case 8:return STN_RPC_CLIENT_PROVIDER;
    case 9:return STN_RPC_CLIENT_CAPACITY;
    case 10:return STN_RPC_CLIENT_STALE;
    default:return STN_RPC_CLIENT_INVALID;
    }
}

static stn_rpc_client_code call(
    stn_rpc_client *client,
    uint16_t method,
    const uint8_t *payload,
    size_t payload_length,
    uint8_t *response_payload,
    size_t response_capacity,
    size_t *written)
{
    uint8_t *request=NULL;
    uint8_t *response=NULL;
    uint64_t request_id;
    uint32_t response_length;
    uint16_t response_code;
    size_t request_capacity;
    size_t response_frame_capacity;
    size_t frame_length=0u;
    stn_rpc_client_code result=STN_RPC_CLIENT_INVALID;

    if(written!=NULL){*written=0;}
    if(client==NULL||client->exchange==NULL||written==NULL||
       payload_length>STN_RPC_CLIENT_MAX_PAYLOAD||
       response_capacity>STN_RPC_CLIENT_MAX_PAYLOAD||
       (payload_length!=0u&&payload==NULL))
    {
        return STN_RPC_CLIENT_INVALID;
    }

    request_capacity=24u+payload_length;
    response_frame_capacity=24u+response_capacity;

    request=(uint8_t *)malloc(request_capacity);
    response=(uint8_t *)malloc(response_frame_capacity);

    if(request==NULL||response==NULL){
        result=STN_RPC_CLIENT_CAPACITY;
        goto done;
    }

    request_id=client->next_request_id++;
    if(request_id==0u){
        request_id=client->next_request_id++;
    }

    memcpy(request,"STNC",4);
    write_be(request+4,2,STN_RPC_CLIENT_VERSION);
    write_be(request+6,2,1u);
    write_be(request+8,2,method);
    write_be(request+10,2,0u);
    write_be(request+12,8,request_id);
    write_be(request+20,4,payload_length);

    if(payload_length!=0u){
        memcpy(request+24,payload,payload_length);
    }

    if(!client->exchange(
        client->transport_user,
        request,
        request_capacity,
        response,
        response_frame_capacity,
        &frame_length))
    {
        result=STN_RPC_CLIENT_TRANSPORT;
        goto done;
    }

    if(frame_length<24u||frame_length>response_frame_capacity||
       memcmp(response,"STNC",4)!=0||
       read_be(response+4,2)!=STN_RPC_CLIENT_VERSION||
       read_be(response+6,2)!=2u||
       read_be(response+8,2)!=method||
       read_be(response+12,8)!=request_id)
    {
        result=STN_RPC_CLIENT_INVALID;
        goto done;
    }

    response_length=(uint32_t)read_be(response+20,4);
    if((size_t)response_length!=frame_length-24u){
        result=STN_RPC_CLIENT_INVALID;
        goto done;
    }

    response_code=(uint16_t)read_be(response+10,2);
    if(response_code!=0u){
        if(response_length!=0u){
            result=STN_RPC_CLIENT_INVALID;
        }
        else{
            result=map_code(response_code);
        }
        goto done;
    }

    if(response_length>response_capacity||
       (response_length!=0u&&response_payload==NULL))
    {
        result=STN_RPC_CLIENT_CAPACITY;
        goto done;
    }

    if((method==STN_RPC_CLIENT_INFO && response_length!=184u) ||
       (method==STN_RPC_CLIENT_SUBMIT_WORK && response_length!=80u) ||
       (method==STN_RPC_CLIENT_MINING_TEMPLATE &&
        (response_length<
             STN_RPC_CLIENT_MINING_PREFIX_SIZE+
             STN_RPC_CLIENT_BLOCK_HEADER_SIZE ||
         read_be(response+24+64,4)!=
             response_length-STN_RPC_CLIENT_MINING_PREFIX_SIZE ||
         memcmp(response+24+STN_RPC_CLIENT_MINING_PREFIX_SIZE,"STNB",4)!=0 ||
         read_be(response+24+STN_RPC_CLIENT_MINING_PREFIX_SIZE+4,2)!=3u)))
    {
        result=STN_RPC_CLIENT_INVALID;
        goto done;
    }

    if(response_length!=0u){
        memcpy(response_payload,response+24,response_length);
    }

    *written=response_length;
    result=STN_RPC_CLIENT_OK;

done:
    free(response);
    free(request);
    return result;
}

void stn_rpc_client_init(
    stn_rpc_client *client,
    void *transport_user,
    stn_rpc_client_exchange_fn exchange)
{
    if(client!=NULL){
        client->transport_user=transport_user;
        client->exchange=exchange;
        client->next_request_id=1u;
    }
}

stn_rpc_client_code stn_rpc_client_info(stn_rpc_client *client,
    uint8_t *payload,size_t capacity,size_t *written)
{
    return call(client,STN_RPC_CLIENT_INFO,NULL,0,payload,capacity,written);
}

stn_rpc_client_code stn_rpc_client_mining_template(
    stn_rpc_client *client,
    uint8_t *payload,
    size_t capacity,
    size_t *written)
{
    return call(
        client,
        STN_RPC_CLIENT_MINING_TEMPLATE,
        NULL,
        0u,
        payload,
        capacity,
        written);
}

stn_rpc_client_code stn_rpc_client_submit_work(
    stn_rpc_client *client,
    const uint8_t *payload,
    size_t length,
    uint8_t *response_payload,
    size_t response_capacity,
    size_t *written)
{
    if(length<68u){
        if(written!=NULL){*written=0;}
        return STN_RPC_CLIENT_INVALID;
    }

    return call(
        client,
        STN_RPC_CLIENT_SUBMIT_WORK,
        payload,
        length,
        response_payload,
        response_capacity,
        written);
}
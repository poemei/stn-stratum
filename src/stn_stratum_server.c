/* Copyright (c) 2026 STN-Labz. All rights reserved. */
#include "stn_stratum_server.h"
#include "stn_share_verify.h"
#include "stn_miner_job.h"
#include "stn_session_acceptor.h"

static void telemetry_tick(stn_stratum_server *server);
static void telemetry_idle(void *user)
{
    telemetry_tick((stn_stratum_server *)user);
}

static void telemetry_hex32(char out[65],const uint8_t value[32])
{
    static const char table[]="0123456789abcdef";
    size_t i;

    for(i=0u;i<32u;i++){
        out[i*2u]=table[(value[i]>>4)&0x0fu];
        out[i*2u+1u]=table[value[i]&0x0fu];
    }
    out[64]='\0';
}

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#include <fcntl.h>
#include <io.h>
#include <share.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#pragma comment(lib,"Ws2_32.lib")

#define STN_INVALID_SOCKET_VALUE ((uintptr_t)INVALID_SOCKET)


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
    for(i=0;i<8u;i++){v=(v<<8)|p[i];}
    return v;
}

static void write32be(uint8_t *p,uint32_t v)
{
    p[0]=(uint8_t)(v>>24);
    p[1]=(uint8_t)(v>>16);
    p[2]=(uint8_t)(v>>8);
    p[3]=(uint8_t)v;
}

static int share_target_from_chain(const uint8_t chain[32],uint8_t share[32])
{
    uint8_t scaled[33]={0};
    unsigned carry=0u;
    size_t i;

    if(chain==NULL || share==NULL){return 0;}
    if(chain[0]>=0x80u){return 0;}
    for(i=0u;i<32u;i++){carry|=chain[i];}
    if(carry==0u){return 0;}

    carry=0u;
    for(i=32u;i!=0u;){
        unsigned v;
        --i;
        v=(unsigned)chain[i]*STN_MINER_SHARE_FACTOR+carry;
        scaled[i+1u]=(uint8_t)(v&0xffu);
        carry=v>>8;
    }
    scaled[0]=(uint8_t)carry;

    if(scaled[0]!=0u || scaled[1]>=0x80u){
        memset(share,0xff,32u);
        share[0]=0x7fu;
    }else{
        memcpy(share,scaled+1u,32u);
    }
    return 1;
}

static void timestamp_utc(char out[32])
{
    time_t now=time(NULL);
    struct tm value;

    if(gmtime_s(&value,&now)!=0){
        strcpy_s(out,32,"0000-00-00T00:00:00Z");
        return;
    }

    (void)strftime(out,32,"%Y-%m-%dT%H:%M:%SZ",&value);
}

static void log_line(stn_stratum_server *server,const char *format,...)
{
    char stamp[32];
    va_list args;
    va_list copy;

    timestamp_utc(stamp);

    printf("[%s] ",stamp);

    va_start(args,format);
    va_copy(copy,args);

    vprintf(format,args);
    printf("\n");
    fflush(stdout);

    if(server!=NULL && server->log_file!=NULL){
        fprintf(server->log_file,"[%s] ",stamp);
        vfprintf(server->log_file,format,copy);
        fprintf(server->log_file,"\n");
        fflush(server->log_file);
    }

    va_end(copy);
    va_end(args);
}

static int log_open(stn_stratum_server *server)
{
    int descriptor=-1;
    FILE *file=NULL;

    if(!CreateDirectoryA("logs",NULL)){
        DWORD e=GetLastError();
        if(e!=ERROR_ALREADY_EXISTS){return 0;}
    }

    if(_sopen_s(
        &descriptor,
        STN_STRATUM_LOG_PATH,
        _O_WRONLY|_O_CREAT|_O_APPEND|_O_BINARY,
        _SH_DENYNO,
        _S_IREAD|_S_IWRITE)!=0)
    {
        return 0;
    }

    file=_fdopen(descriptor,"ab");
    if(file==NULL){
        _close(descriptor);
        return 0;
    }

    server->log_file=file;
    return 1;
}

static void log_close(stn_stratum_server *server)
{
    if(server->log_file!=NULL){
        fclose(server->log_file);
        server->log_file=NULL;
    }
}

static void hex8(char out[17],const uint8_t value[32])
{
    static const char table[]="0123456789abcdef";
    size_t i;

    for(i=0;i<8u;i++){
        out[i*2u]=table[(value[i]>>4)&0x0fu];
        out[i*2u+1u]=table[value[i]&0x0fu];
    }
    out[16]='\0';
}

static int winsock_open(stn_stratum_server *server)
{
    WSADATA wsa;

    if(server->winsock_ready){return 1;}

    if(WSAStartup(MAKEWORD(2,2),&wsa)!=0){return 0;}

    server->winsock_ready=1;
    return 1;
}

static void winsock_close(stn_stratum_server *server)
{
    if(server->winsock_ready){
        WSACleanup();
        server->winsock_ready=0;
    }
}

static int send_all(SOCKET s,const uint8_t *p,size_t n)
{
    while(n!=0u){
        int sent=send(s,(const char *)p,(int)n,0);
        if(sent<=0){return 0;}
        p+=(size_t)sent;
        n-=(size_t)sent;
    }
    return 1;
}

static void client_remove(
    stn_stratum_server *server,
    stn_stratum_client_session *client,
    const char *reason)
{
    stn_stratum_client_session **link;

    if(server==NULL || client==NULL){return;}

    link=&server->clients;
    while(*link!=NULL && *link!=client){
        link=&(*link)->next;
    }

    if(*link==NULL){return;}

    *link=client->next;


    if(server->client_count!=0u){
        server->client_count--;
    }

    if(reason!=NULL){
        log_line(
            server,
            "CLIENT disconnected: %s; clients=%zu.",
            reason,
            server->client_count);
    }

    stn_session_acceptor_release(client);
}

static stn_chain_config_entry *active_chain(
    stn_stratum_server *server);

static void telemetry_tick(stn_stratum_server *server)
{
    stn_telemetry_status status;
    stn_chain_config_entry *entry;
    char job[65];
    char base[65];
    char target[65];
    time_t now;

    if(server==NULL){return;}

    memset(&status,0,sizeof(status));
    memset(job,'0',64u); job[64]='\0';
    memset(base,'0',64u); base[64]='\0';
    memset(target,'0',64u); target[64]='\0';

    if(server->have_work){
        telemetry_hex32(job,server->active_job_id);
        telemetry_hex32(base,server->active_base);
        if(server->active_template_length>=
            68u+STN_MINER_CHAIN_TARGET_OFFSET+32u)
        {
            telemetry_hex32(target,
                server->active_template+68u+STN_MINER_CHAIN_TARGET_OFFSET);
        }
    }

    entry=active_chain(server);
    status.chain_connected=server->chain_available;
    status.chain_host=entry!=NULL?entry->host:"";
    status.chain_port=entry!=NULL?entry->port:0u;
    status.work_available=server->have_work;
    status.miners=stn_client_session_count(server->clients);
    status.hashrate=stn_client_session_total_hashrate(server->clients);
    status.job_id=job;
    status.base_id=base;
    status.target=target;

    now=time(NULL);
    if(server->started_at!=(time_t)0 && now>=server->started_at){
        status.uptime_seconds=(uint64_t)(now-server->started_at);
    }

    stn_telemetry_service_tick(&server->telemetry,&status,now);
}

static int listeners_open(stn_stratum_server *server)
{
    if(server==NULL){return 0;}

#ifdef _WIN32
    if(!winsock_open(server)){
        log_line(server,"ERROR Winsock initialization failed.");
        return 0;
    }
#endif

    stn_mining_listener_init(&server->pool_listener,STN_SERVICE_MODE_POOL);
    stn_mining_listener_init(&server->solo_listener,STN_SERVICE_MODE_SOLO);

    if(!stn_mining_listener_open(&server->pool_listener)){
        log_line(server,"ERROR unable to open Pool listener on port %u.",
            (unsigned)server->pool_listener.port);
        return 0;
    }

    if(!stn_mining_listener_open(&server->solo_listener)){
        log_line(server,"ERROR unable to open Solo listener on port %u.",
            (unsigned)server->solo_listener.port);
        stn_mining_listener_close(&server->pool_listener);
        return 0;
    }

    log_line(server,"LISTEN Pool 0.0.0.0:%u ready.",
        (unsigned)server->pool_listener.port);
    log_line(server,"LISTEN Solo 0.0.0.0:%u ready.",
        (unsigned)server->solo_listener.port);
    return 1;
}

static void listeners_close(stn_stratum_server *server)
{
    if(server==NULL){return;}

    stn_mining_listener_close(&server->solo_listener);
    stn_mining_listener_close(&server->pool_listener);

    log_line(server,"LISTEN Pool/Solo listeners closed.");
}

static int client_send_job(
    stn_stratum_server *server,
    stn_stratum_client_session *client)
{
    uint8_t header[STN_MINER_JOB_HEADER_SIZE];
    uint32_t block_length;
    SOCKET socket;

    if(client==NULL || !server->have_work){return 1;}

    socket=(SOCKET)client->socket;
    if(socket==INVALID_SOCKET){return 0;}

    if(!stn_miner_job_build(server->active_template,
            server->active_template_length,header,&block_length))
    {
        log_line(server,"ERROR active Chain template cannot form Miner JOB.");
        return 0;
    }

    if(!send_all(socket,header,sizeof(header)) ||
       !send_all(socket,server->active_template+68u,block_length))
    {
        client_remove(server,client,"job send failed");
        return 0;
    }

    memcpy(client->sent_job_id,server->active_job_id,32);
    client->has_job=1;

    {
        char job[17];
        hex8(job,server->active_job_id);
        log_line(server,"CLIENT job sent job=%s... block=%u bytes.",
            job,(unsigned)block_length);
    }

    return 1;
}

static void accept_from_listener(
    stn_stratum_server *server,
    const stn_mining_listener *listener)
{
    for(;;){
        stn_stratum_client_session *client=NULL;
        int result=stn_session_acceptor_accept(listener,&client);

        if(result==0){return;}
        if(result<0){
            log_line(server,"WARN accept failed on %s port %u.",
                listener->mode==STN_SERVICE_MODE_SOLO?"Solo":"Pool",
                (unsigned)listener->port);
            return;
        }

        client->next=server->clients;
        server->clients=client;
        server->client_count++;

        log_line(server,"CLIENT connected mode=%s port=%u; clients=%zu.",
            stn_client_mode_is_solo(&client->mode)?"solo":"pool",
            (unsigned)listener->port,
            server->client_count);
    }
}

static void accept_clients(stn_stratum_server *server)
{
    accept_from_listener(server,&server->pool_listener);
    accept_from_listener(server,&server->solo_listener);
}

static uint32_t submit_result_code(stn_rpc_client_code code)
{
    switch(code){
    case STN_RPC_CLIENT_OK:return STN_MINER_RESULT_ACCEPTED;
    case STN_RPC_CLIENT_STALE:return STN_MINER_RESULT_STALE;
    case STN_RPC_CLIENT_REJECTED:return STN_MINER_RESULT_REJECTED;
    case STN_RPC_CLIENT_INVALID:
    case STN_RPC_CLIENT_VERSION_ERROR:
        return STN_MINER_RESULT_PROTOCOL;
    default:
        return STN_MINER_RESULT_PROVIDER;
    }
}

static void client_send_result(
    stn_stratum_server *server,
    stn_stratum_client_session *client,
    stn_rpc_client_code rpc_code)
{
    uint8_t response[STN_MINER_RESULT_SIZE];
    SOCKET socket;

    if(client==NULL){return;}

    socket=(SOCKET)client->socket;
    if(socket==INVALID_SOCKET){return;}

    memset(response,0,sizeof(response));
    memcpy(response,STN_MINER_MAGIC,4);
    response[4]=STN_MINER_VERSION;
    response[5]=STN_MINER_RESULT;
    write32be(response+8,submit_result_code(rpc_code));

    if(!send_all(socket,response,sizeof(response))){
        client_remove(server,client,"result send failed");
    }
}

static void clear_client_jobs(stn_stratum_server *server)
{
    stn_stratum_client_session *client;

    for(client=server->clients;client!=NULL;client=client->next){
        client->has_job=0;
        client->hashrate=0u;
        memset(client->sent_job_id,0,sizeof(client->sent_job_id));
    }
}

static void clear_work(stn_stratum_server *server)
{
    memset(server->active_base,0,sizeof(server->active_base));
    memset(server->active_job_id,0,sizeof(server->active_job_id));
    server->active_template_length=0u;
    server->have_work=0;
    clear_client_jobs(server);
}

static stn_chain_config_entry *active_chain(
    stn_stratum_server *server)
{
    if(server==NULL ||
       server->chain_config.server_count==0u ||
       server->active_chain_server>=server->chain_config.server_count)
    {
        return NULL;
    }

    return &server->chain_config.servers[server->active_chain_server];
}

static int chain_transport_select(
    stn_stratum_server *server,
    size_t index)
{
    stn_chain_config_entry *entry;

    if(server==NULL ||
       index>=server->chain_config.server_count)
    {
        return 0;
    }

    clear_work(server);
    server->chain_available=0;

    stn_rpc_win32_close(&server->chain_transport);

    server->active_chain_server=index;
    entry=&server->chain_config.servers[index];

    stn_rpc_win32_init(
        &server->chain_transport,
        entry->host,
        entry->port);

    server->chain_transport.idle=telemetry_idle;
    server->chain_transport.idle_user=server;

    stn_rpc_client_init(
        &server->chain_rpc,
        &server->chain_transport,
        stn_rpc_win32_exchange);

    log_line(server,
        "CHAIN target %s:%u.",
        entry->host,
        (unsigned)entry->port);

    return 1;
}

static int chain_transport_next(
    stn_stratum_server *server)
{
    size_t next;

    if(server==NULL ||
       server->chain_config.server_count==0u)
    {
        return 0;
    }

    next=server->active_chain_server+1u;

    if(next>=server->chain_config.server_count){
        next=0u;
    }

    return chain_transport_select(server,next);
}

static void process_submission(
    stn_stratum_server *server,
    stn_stratum_client_session *client)
{
    const uint8_t *p;
    uint8_t *payload;
    uint8_t response[80];
    size_t response_length=0u;
    uint32_t block_length;
    uint64_t nonce;
    stn_rpc_client_code code;
    stn_share_class share_class;
    uint8_t share_hash[32];

    if(client==NULL){return;}
    if(!client->address_registered){
        log_line(server,"CLIENT SUBMIT before ADDRESS.");
        client_remove(server,client,"ADDRESS required");
        return;
    }
    p=client->rx;

    if(memcmp(p,STN_MINER_MAGIC,4)!=0 ||
       p[4]!=STN_MINER_VERSION ||
       p[5]!=STN_MINER_SUBMIT || p[6]!=0 || p[7]!=0)
    {
        log_line(server,"CLIENT invalid submit frame.");
        client_send_result(server,client,STN_RPC_CLIENT_INVALID);
        return;
    }

    if(!server->have_work ||
       memcmp(p+8,server->active_job_id,32)!=0)
    {
        log_line(server,"CLIENT stale job submission.");
        client_send_result(server,client,STN_RPC_CLIENT_STALE);
        return;
    }

    if(server->active_template_length<68u){
        log_line(server,"ERROR active Chain template invalid during submit.");
        client_send_result(server,client,STN_RPC_CLIENT_INVALID);
        return;
    }

    block_length=read32be(server->active_template+64);

    if((size_t)block_length!=server->active_template_length-68u ||
       block_length<STN_MINER_CHAIN_HEADER_SIZE)
    {
        log_line(server,"ERROR active Chain template invalid during submit.");
        client_send_result(server,client,STN_RPC_CLIENT_INVALID);
        return;
    }

    nonce=read64be(p+40);

    {
        uint8_t share_target[32];
        const uint8_t *chain_target=
            server->active_template+68u+STN_MINER_CHAIN_TARGET_OFFSET;

        if(!share_target_from_chain(chain_target,share_target)){
            log_line(server,"ERROR active Chain target invalid during submit.");
            client_send_result(server,client,STN_RPC_CLIENT_INVALID);
            return;
        }

        share_class=stn_share_classify(
            server->active_template+68u,
            block_length,
            nonce,
            share_target,
            chain_target,
            share_hash);

        if(share_class==STN_SHARE_INVALID){
            log_line(server,"CLIENT submit rejected: proof exceeds Share Target.");
            client_send_result(server,client,STN_RPC_CLIENT_REJECTED);
            return;
        }

        if(share_class==STN_SHARE_QUALIFYING){
            uint8_t share_payload[STN_RPC_CLIENT_SHARE_SIZE];
            uint8_t share_id[32];
            stn_rpc_client_code share_code;
            char share_hex[17];

            memcpy(share_payload,server->active_job_id,32u);
            memcpy(share_payload+32u,client->address,STN_MINER_ADDRESS_LENGTH);
            {
                size_t i;
                uint64_t value=nonce;
                for(i=0u;i<8u;i++){
                    share_payload[108u-i]=(uint8_t)(value&0xffu);
                    value>>=8;
                }
            }
            memcpy(share_payload+STN_RPC_CLIENT_SHARE_PREFIX_SIZE,
                server->active_template+68u,
                STN_MINER_CHAIN_HEADER_SIZE);

            share_code=stn_rpc_client_submit_share(
                &server->chain_rpc,
                share_payload,
                sizeof(share_payload),
                share_id);

            if(share_code==STN_RPC_CLIENT_OK){
                hex8(share_hex,share_id);
                log_line(server,
                    "CLIENT qualifying share nonce=%llu Chain verified share=%s....",
                    (unsigned long long)nonce,share_hex);
            }else{
                log_line(server,
                    "CLIENT qualifying share nonce=%llu Chain result=%d.",
                    (unsigned long long)nonce,(int)share_code);
            }

            client_send_result(server,client,share_code);
            /*
             * An accepted qualifying share does not consume the active Work ID.
             * Other miners, and this miner, may continue producing independent
             * qualifying evidence against the same immutable Chain work.
             * Clear only when Chain says the work/session can no longer be used.
             */
            if(share_code==STN_RPC_CLIENT_STALE ||
               share_code==STN_RPC_CLIENT_TRANSPORT ||
               share_code==STN_RPC_CLIENT_UNAVAILABLE ||
               share_code==STN_RPC_CLIENT_PROVIDER ||
               share_code==STN_RPC_CLIENT_INVALID){
                clear_work(server);
            }
            return;
        }
    }

    payload=(uint8_t *)malloc(
        server->active_template_length+STN_MINER_ADDRESS_LENGTH);
    if(payload==NULL){
        log_line(server,
            "ERROR submission scratch allocation failed: %zu bytes.",
            server->active_template_length);
        client_send_result(server,client,STN_RPC_CLIENT_PROVIDER);
        return;
    }

    memcpy(payload,server->active_template,68u);
    memcpy(payload+68u,client->address,STN_MINER_ADDRESS_LENGTH);
    memcpy(
        payload+68u+STN_MINER_ADDRESS_LENGTH,
        server->active_template+68u,
        server->active_template_length-68u);

    {
        uint8_t *nonce_field=
            payload+68u+STN_MINER_ADDRESS_LENGTH+
            STN_MINER_CHAIN_NONCE_OFFSET;
        size_t i;
        uint64_t value=nonce;

        for(i=0;i<8u;i++){
            nonce_field[7u-i]=(uint8_t)(value&0xffu);
            value>>=8;
        }
    }

    code=stn_rpc_client_submit_work(
        &server->chain_rpc,
        payload,
        server->active_template_length+STN_MINER_ADDRESS_LENGTH,
        response,
        sizeof(response),
        &response_length);

    free(payload);

    log_line(server,"CLIENT submit nonce=%llu Chain result=%d response=%zu.",
        (unsigned long long)nonce,(int)code,response_length);

    client_send_result(server,client,code);

    /*
     * A solved block changes Chain state, but do not invalidate the miner's
     * session job locally. Chain is authoritative for staleness. Keeping the
     * sent job until the next template poll prevents a just-submitted solution
     * from turning an already-in-flight miner SUBMIT into an artificial
     * Stratum-side stale result.
     */
    /*
     * Submission results do not invalidate Stratum's active job locally.
     * Chain owns work lifetime.  The polling path replaces work when Chain
     * publishes a new template or explicitly reports the current work stale.
     * Keeping the job here also prevents miners from being stranded in
     * WAIT_JOB while Chain transitions to replacement work.
     */
    if(code==STN_RPC_CLIENT_TRANSPORT)
    {
        server->chain_available=0;
    }

    if(code==STN_RPC_CLIENT_TRANSPORT ||
       code==STN_RPC_CLIENT_UNAVAILABLE)
    {
        log_line(server,
            "CHAIN RPC endpoint unavailable after submission; "
            "selecting next configured server.");

        (void)chain_transport_next(server);
    }
}

static int address_valid(const uint8_t *p)
{
    size_t i;
    if(p==NULL){return 0;}
    if(memcmp(p,STN_MINER_MAGIC,4)!=0 ||
       p[4]!=STN_MINER_VERSION ||
       p[5]!=STN_MINER_ADDRESS ||
       p[6]!=0 || p[7]!=0){return 0;}
    if(memcmp(p+8,"stn0_",5)!=0){return 0;}
    for(i=13u;i<8u+STN_MINER_ADDRESS_LENGTH;i++){
        if(!((p[i]>=(uint8_t)'0' && p[i]<=(uint8_t)'9') ||
             (p[i]>=(uint8_t)'a' && p[i]<=(uint8_t)'f'))){return 0;}
    }
    return 1;
}

static void process_address(
    stn_stratum_server *server,
    stn_stratum_client_session *client)
{
    if(client==NULL){return;}
    if(client->address_registered){
        log_line(server,"CLIENT duplicate ADDRESS rejected.");
        client_remove(server,client,"duplicate ADDRESS");
        return;
    }
    if(!address_valid(client->rx)){
        log_line(server,"CLIENT invalid ADDRESS frame.");
        client_remove(server,client,"invalid ADDRESS");
        return;
    }
    memcpy(client->address,client->rx+8,STN_MINER_ADDRESS_LENGTH);
    client->address[STN_MINER_ADDRESS_LENGTH]='\0';
    client->address_registered=1;
    log_line(server,"CLIENT address registered: %s.",client->address);
    if(server->have_work){(void)client_send_job(server,client);}
}

static void process_hash_progress(
    stn_stratum_server *server,
    stn_stratum_client_session *client)
{
    const uint8_t *p;
    uint64_t hashes;
    uint64_t elapsed_ms;
    uint64_t quotient;
    uint64_t remainder;
    uint64_t rate;

    if(server==NULL || client==NULL){return;}
    if(!client->address_registered){
        log_line(server,"CLIENT HASH_PROGRESS before ADDRESS.");
        client_remove(server,client,"ADDRESS required");
        return;
    }

    p=client->rx;

    if(memcmp(p,STN_MINER_MAGIC,4)!=0 ||
       p[4]!=STN_MINER_VERSION ||
       p[5]!=STN_MINER_HASH_PROGRESS ||
       p[6]!=0 ||
       p[7]!=0)
    {
        return;
    }

    if(!server->have_work ||
       !client->has_job ||
       memcmp(p+8,server->active_job_id,32)!=0 ||
       memcmp(p+8,client->sent_job_id,32)!=0)
    {
        client->hashrate=0u;
        return;
    }

    hashes=read64be(p+40);
    elapsed_ms=read64be(p+48);

    if(elapsed_ms==0u){
        client->hashrate=0u;
        return;
    }

    quotient=hashes/elapsed_ms;
    remainder=hashes%elapsed_ms;

    if(quotient>UINT64_MAX/1000u){
        rate=UINT64_MAX;
    }
    else{
        rate=quotient*1000u;

        {
            uint64_t extra;

            if(remainder<=UINT64_MAX/1000u){
                extra=(remainder*1000u)/elapsed_ms;
            }
            else{
                uint64_t scaled_divisor=(elapsed_ms/1000u)+1u;
                extra=remainder/scaled_divisor;
                if(extra>999u){extra=999u;}
            }

            if(UINT64_MAX-rate<extra){
                rate=UINT64_MAX;
            }
            else{
                rate+=extra;
            }
        }
    }

    client->hashrate=rate;
}

static void service_client(
    stn_stratum_server *server,
    stn_stratum_client_session *client)
{
    SOCKET socket;
    size_t frame_size;

    if(client==NULL){return;}

    socket=(SOCKET)client->socket;
    if(socket==INVALID_SOCKET){return;}

    for(;;){
        if(client->rx_used<8u){
            frame_size=8u;
        }
        else if(memcmp(client->rx,STN_MINER_MAGIC,4)!=0 ||
                client->rx[4]!=STN_MINER_VERSION ||
                client->rx[6]!=0 ||
                client->rx[7]!=0)
        {
            client_remove(server,client,"invalid frame header");
            return;
        }
        else if(client->rx[5]==STN_MINER_ADDRESS){
            frame_size=STN_MINER_ADDRESS_FRAME_SIZE;
        }
        else if(client->rx[5]==STN_MINER_SUBMIT){
            frame_size=STN_MINER_SUBMIT_SIZE;
        }
        else if(client->rx[5]==STN_MINER_HASH_PROGRESS){
            frame_size=STN_MINER_HASH_PROGRESS_SIZE;
        }
        else{
            client_remove(server,client,"unsupported frame type");
            return;
        }

        while(client->rx_used<frame_size){
            int got=recv(
            socket,
            (char *)client->rx+client->rx_used,
            (int)(frame_size-client->rx_used),
            0);

            if(got>0){
                client->rx_used+=(size_t)got;
                continue;
            }

            if(got==0){
                client_remove(server,client,"peer closed");
                return;
            }

        if(WSAGetLastError()==WSAEWOULDBLOCK){break;}

        client_remove(server,client,"receive failed");
        return;
        }

        if(client->rx_used<frame_size){
            return;
        }

        if(client->rx[5]==STN_MINER_ADDRESS){
            process_address(server,client);
        }
        else if(client->rx[5]==STN_MINER_SUBMIT){
            process_submission(server,client);
        }
        else{
            process_hash_progress(server,client);
        }

        {
            stn_stratum_client_session *probe;
            int still_connected=0;

            for(probe=server->clients;probe!=NULL;probe=probe->next){
                if(probe==client){
                    still_connected=1;
                    break;
                }
            }

            if(!still_connected){
                return;
            }
        }

        client->rx_used=0u;
        /* One complete frame per pass: a busy miner must yield to peers
         * and the Chain work poll instead of draining an unbounded stream. */
        return;
    }
}

static void service_clients(stn_stratum_server *server)
{
    stn_stratum_client_session *client=server->clients;

    while(client!=NULL){
        stn_stratum_client_session *next=client->next;
        service_client(server,client);
        client=next;
    }
}

static void send_pending_jobs(stn_stratum_server *server)
{
    stn_stratum_client_session *client=server->clients;

    while(client!=NULL){
        stn_stratum_client_session *next=client->next;

        if(server->have_work &&
           client->address_registered &&
           (!client->has_job ||
            memcmp(client->sent_job_id,server->active_job_id,32)!=0))
        {
            (void)client_send_job(server,client);
        }

        client=next;
    }
}

static void refresh_work(stn_stratum_server *server)
{
    uint8_t *payload=server->active_template;
    size_t written=0u;
    uint32_t block_length;
    stn_rpc_client_code code;
    stn_chain_config_entry *entry;
    int changed;

    code=stn_rpc_client_mining_template(
        &server->chain_rpc,
        payload,
        sizeof(server->active_template),
        &written);

    server->chain_poll_count++;

    if(code!=STN_RPC_CLIENT_OK){
        if(code==STN_RPC_CLIENT_STALE){
            log_line(server,
                "CHAIN work stale; invalidating current job and refreshing.");
            server->chain_available=1;
            clear_work(server);
            return;
        }

        if(code==STN_RPC_CLIENT_CAPACITY){
            log_line(server,
                "ERROR CHAIN RPC capacity failure code=%d; "
                "connection remains available; retrying.",
                (int)code);
            server->chain_available=1;
            clear_work(server);
            return;
        }

        if(code==STN_RPC_CLIENT_UNAVAILABLE){
            if(!server->chain_available || server->have_work ||
               (server->chain_poll_count%STN_STRATUM_HEARTBEAT_CHAIN_POLLS)==1u){
                log_line(server,
                    "CHAIN RPC reachable; mining template unavailable code=5; "
                    "retaining server and retrying.");
            }
            server->chain_available=1;
            /*
             * Chain is reachable but has no replacement template yet.
             * Retain the current immutable job until Chain supplies new work
             * or explicitly reports it stale.
             */
            return;
        }

        if(code==STN_RPC_CLIENT_TRANSPORT)
        {
            if(server->chain_available){
                log_line(server,
                    "CHAIN RPC connection unavailable code=%d; "
                    "work invalidated.",
                    (int)code);
            }
            else if((server->chain_poll_count%
                     STN_STRATUM_HEARTBEAT_CHAIN_POLLS)==1u)
            {
                log_line(server,
                    "CHAIN RPC connection still unavailable code=%d; "
                    "selecting next configured server.",
                    (int)code);
            }

            server->chain_available=0;
            clear_work(server);
            (void)chain_transport_next(server);
            return;
        }

        log_line(server,
            "ERROR CHAIN mining-template RPC rejected code=%d; retrying.",
            (int)code);
        server->chain_available=1;
        clear_work(server);
        return;
    }

    if(written<68u){
        log_line(server,
            "ERROR CHAIN mining template malformed: %zu bytes.",
            written);
        server->chain_available=0;
        clear_work(server);
        return;
    }

    block_length=read32be(payload+64);

    if((size_t)block_length!=written-68u){
        log_line(server,
            "ERROR CHAIN template length mismatch frame=%zu block=%u.",
            written,
            (unsigned)block_length);
        server->chain_available=0;
        clear_work(server);
        return;
    }

    if(!server->chain_available){
        entry=active_chain(server);

        if(entry!=NULL){
            log_line(server,
                "CHAIN RPC connected %s:%u.",
                entry->host,
                (unsigned)entry->port);
        }
    }

    server->chain_available=1;

    changed=!server->have_work ||
        memcmp(server->active_base,payload,32)!=0 ||
        memcmp(server->active_job_id,payload+32,32)!=0;

    server->active_template_length=written;

    if(changed){
        char base[17];
        char job[17];

        memcpy(server->active_base,payload,32);
        memcpy(server->active_job_id,payload+32,32);
        server->have_work=1;

        hex8(base,server->active_base);
        hex8(job,server->active_job_id);

        log_line(server,
            "WORK new base=%s... job=%s... block=%u bytes.",
            base,
            job,
            (unsigned)block_length);

        send_pending_jobs(server);
        return;
    }

    if((server->chain_poll_count%
        STN_STRATUM_HEARTBEAT_CHAIN_POLLS)==0u)
    {
        char base[17];
        char job[17];

        hex8(base,server->active_base);
        hex8(job,server->active_job_id);

        log_line(server,
            "HEARTBEAT running chain=connected work=active clients=%zu "
            "base=%s... job=%s....",
            server->client_count,
            base,
            job);
    }
}

void stn_stratum_server_init(stn_stratum_server *server)
{
    if(server==NULL){return;}

    memset(server,0,sizeof(*server));
    stn_mining_listener_init(&server->pool_listener,STN_SERVICE_MODE_POOL);
    stn_mining_listener_init(&server->solo_listener,STN_SERVICE_MODE_SOLO);
    stn_telemetry_service_init(&server->telemetry,STN_STRATUM_TELEMETRY_PORT);
    server->started_at=time(NULL);
    server->running=1;
}

int stn_stratum_server_run(stn_stratum_server *server)
{
    stn_chain_config_entry *entry;

    if(server==NULL){return 0;}

    if(!log_open(server)){
        fprintf(stderr,
            "Unable to open local log: %s\n",
            STN_STRATUM_LOG_PATH);
        return 0;
    }

    log_line(server,"START STN-Stratum development server.");
    log_line(server,"LOG %s.",STN_STRATUM_LOG_PATH);
    log_line(server,"CHAIN config %s.",STN_STRATUM_CHAIN_CONFIG);

    if(!stn_chain_config_load(
        &server->chain_config,
        STN_STRATUM_CHAIN_CONFIG))
    {
        log_line(server,
            "ERROR unable to load Chain configuration: %s.",
            STN_STRATUM_CHAIN_CONFIG);
        log_line(server,"STOP startup failed.");
        return 0;
    }

    if(server->chain_config.server_count==0u){
        log_line(server,
            "ERROR Chain configuration contains no servers.");
        log_line(server,"STOP startup failed.");
        return 0;
    }

    server->active_chain_server=0u;

    if(!chain_transport_select(server,0u)){
        log_line(server,
            "ERROR unable to select configured Chain server.");
        log_line(server,"STOP startup failed.");
        return 0;
    }

    entry=active_chain(server);

    if(entry==NULL){
        log_line(server,
            "ERROR configured Chain server unavailable.");
        log_line(server,"STOP startup failed.");
        return 0;
    }

    log_line(server,
        "CHAIN servers loaded=%zu.",
        server->chain_config.server_count);

    if(!listeners_open(server)){
        log_line(server,"STOP startup failed.");
        return 0;
    }

    if(!stn_telemetry_service_open(&server->telemetry)){
        log_line(server,"ERROR unable to open telemetry listener on port %u.",
            (unsigned)STN_STRATUM_TELEMETRY_PORT);
        listeners_close(server);
        log_line(server,"STOP startup failed.");
        return 0;
    }

    log_line(server,"TELEMETRY 0.0.0.0:%u ready.",
        (unsigned)STN_STRATUM_TELEMETRY_PORT);

    while(server->running){
        accept_clients(server);
        service_clients(server);
        telemetry_tick(server);

        if((server->loop_count%
            STN_STRATUM_CHAIN_POLL_TICKS)==0u)
        {
            refresh_work(server);
        }

        send_pending_jobs(server);

        server->loop_count++;
        Sleep(STN_STRATUM_POLL_MS);
    }

    log_line(server,"STOP requested.");
    return 1;
}

void stn_stratum_server_stop(stn_stratum_server *server)
{
    if(server!=NULL){server->running=0;}
}

void stn_stratum_server_close(stn_stratum_server *server)
{
    if(server==NULL){return;}

    while(server->clients!=NULL){
        client_remove(server,server->clients,NULL);
    }

    stn_telemetry_service_close(&server->telemetry);
    listeners_close(server);
    stn_rpc_win32_close(&server->chain_transport);
    winsock_close(server);

    clear_work(server);
    server->chain_available=0;

    stn_chain_config_close(&server->chain_config);

    log_line(server,"STOP complete.");
    log_close(server);
}

#elif defined(__linux__)

#define _POSIX_C_SOURCE 200809L

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define STN_INVALID_SOCKET_VALUE ((uintptr_t)-1)



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

    for(i=0u;i<8u;i++){
        v=(v<<8)|p[i];
    }

    return v;
}

static void write32be(uint8_t *p,uint32_t v)
{
    p[0]=(uint8_t)(v>>24);
    p[1]=(uint8_t)(v>>16);
    p[2]=(uint8_t)(v>>8);
    p[3]=(uint8_t)v;
}

static int share_target_from_chain(const uint8_t chain[32],uint8_t share[32])
{
    uint8_t scaled[33]={0};
    unsigned carry=0u;
    size_t i;

    if(chain==NULL || share==NULL){return 0;}
    if(chain[0]>=0x80u){return 0;}
    for(i=0u;i<32u;i++){carry|=chain[i];}
    if(carry==0u){return 0;}

    carry=0u;
    for(i=32u;i!=0u;){
        unsigned v;
        --i;
        v=(unsigned)chain[i]*STN_MINER_SHARE_FACTOR+carry;
        scaled[i+1u]=(uint8_t)(v&0xffu);
        carry=v>>8;
    }
    scaled[0]=(uint8_t)carry;

    if(scaled[0]!=0u || scaled[1]>=0x80u){
        memset(share,0xff,32u);
        share[0]=0x7fu;
    }else{
        memcpy(share,scaled+1u,32u);
    }
    return 1;
}

static void timestamp_utc(char out[32])
{
    time_t now=time(NULL);
    struct tm value;

    if(gmtime_r(&now,&value)==NULL){
        (void)snprintf(out,32,"0000-00-00T00:00:00Z");
        return;
    }

    (void)strftime(out,32,"%Y-%m-%dT%H:%M:%SZ",&value);
}

static void log_line(stn_stratum_server *server,const char *format,...)
{
    char stamp[32];
    va_list args;
    va_list copy;

    timestamp_utc(stamp);

    printf("[%s] ",stamp);

    va_start(args,format);
    va_copy(copy,args);

    vprintf(format,args);
    printf("\n");
    fflush(stdout);

    if(server!=NULL && server->log_file!=NULL){
        fprintf(server->log_file,"[%s] ",stamp);
        vfprintf(server->log_file,format,copy);
        fprintf(server->log_file,"\n");
        fflush(server->log_file);
    }

    va_end(copy);
    va_end(args);
}

static int log_open(stn_stratum_server *server)
{
    server->log_file=fopen(STN_STRATUM_LOG_PATH,"a");

    if(server->log_file==NULL){
        return 0;
    }

    return 1;
}

static void log_close(stn_stratum_server *server)
{
    if(server->log_file!=NULL){
        fclose(server->log_file);
        server->log_file=NULL;
    }
}

static void hex8(char out[17],const uint8_t value[32])
{
    static const char table[]="0123456789abcdef";
    size_t i;

    for(i=0u;i<8u;i++){
        out[i*2u]=table[(value[i]>>4)&0x0fu];
        out[i*2u+1u]=table[value[i]&0x0fu];
    }

    out[16]='\0';
}

static int send_all(int socket,const uint8_t *p,size_t n)
{
    while(n!=0u){
        ssize_t sent=send(socket,p,n,MSG_NOSIGNAL);

        if(sent<0){
            if(errno==EINTR){
                continue;
            }

            return 0;
        }

        if(sent==0){
            return 0;
        }

        p+=(size_t)sent;
        n-=(size_t)sent;
    }

    return 1;
}

static void client_remove(
    stn_stratum_server *server,
    stn_stratum_client_session *client,
    const char *reason)
{
    stn_stratum_client_session **link;

    if(server==NULL || client==NULL){return;}

    link=&server->clients;

    while(*link!=NULL && *link!=client){
        link=&(*link)->next;
    }

    if(*link==NULL){return;}

    *link=client->next;


    if(server->client_count!=0u){
        server->client_count--;
    }

    if(reason!=NULL){
        log_line(
            server,
            "CLIENT disconnected: %s; clients=%zu.",
            reason,
            server->client_count);
    }

    stn_session_acceptor_release(client);
}


static stn_chain_config_entry *active_chain(
    stn_stratum_server *server);

static void telemetry_tick(stn_stratum_server *server)
{
    stn_telemetry_status status;
    stn_chain_config_entry *entry;
    char job[65];
    char base[65];
    char target[65];
    time_t now;

    if(server==NULL){return;}

    memset(&status,0,sizeof(status));
    memset(job,'0',64u); job[64]='\0';
    memset(base,'0',64u); base[64]='\0';
    memset(target,'0',64u); target[64]='\0';

    if(server->have_work){
        telemetry_hex32(job,server->active_job_id);
        telemetry_hex32(base,server->active_base);
        if(server->active_template_length>=
            68u+STN_MINER_CHAIN_TARGET_OFFSET+32u)
        {
            telemetry_hex32(target,
                server->active_template+68u+STN_MINER_CHAIN_TARGET_OFFSET);
        }
    }

    entry=active_chain(server);
    status.chain_connected=server->chain_available;
    status.chain_host=entry!=NULL?entry->host:"";
    status.chain_port=entry!=NULL?entry->port:0u;
    status.work_available=server->have_work;
    status.miners=stn_client_session_count(server->clients);
    status.hashrate=stn_client_session_total_hashrate(server->clients);
    status.job_id=job;
    status.base_id=base;
    status.target=target;

    now=time(NULL);
    if(server->started_at!=(time_t)0 && now>=server->started_at){
        status.uptime_seconds=(uint64_t)(now-server->started_at);
    }

    stn_telemetry_service_tick(&server->telemetry,&status,now);
}

static int listeners_open(stn_stratum_server *server)
{
    if(server==NULL){return 0;}

#ifdef _WIN32
    if(!winsock_open(server)){
        log_line(server,"ERROR Winsock initialization failed.");
        return 0;
    }
#endif

    stn_mining_listener_init(&server->pool_listener,STN_SERVICE_MODE_POOL);
    stn_mining_listener_init(&server->solo_listener,STN_SERVICE_MODE_SOLO);

    if(!stn_mining_listener_open(&server->pool_listener)){
        log_line(server,"ERROR unable to open Pool listener on port %u.",
            (unsigned)server->pool_listener.port);
        return 0;
    }

    if(!stn_mining_listener_open(&server->solo_listener)){
        log_line(server,"ERROR unable to open Solo listener on port %u.",
            (unsigned)server->solo_listener.port);
        stn_mining_listener_close(&server->pool_listener);
        return 0;
    }

    log_line(server,"LISTEN Pool 0.0.0.0:%u ready.",
        (unsigned)server->pool_listener.port);
    log_line(server,"LISTEN Solo 0.0.0.0:%u ready.",
        (unsigned)server->solo_listener.port);
    return 1;
}

static void listeners_close(stn_stratum_server *server)
{
    if(server==NULL){return;}

    stn_mining_listener_close(&server->solo_listener);
    stn_mining_listener_close(&server->pool_listener);

    log_line(server,"LISTEN Pool/Solo listeners closed.");
}

static int client_send_job(
    stn_stratum_server *server,
    stn_stratum_client_session *client)
{
    uint8_t header[STN_MINER_JOB_HEADER_SIZE];
    uint32_t block_length;
    int socket_fd;

    if(client==NULL || !server->have_work){
        return 1;
    }

    socket_fd=(int)client->socket;

    if(client->socket==STN_INVALID_SOCKET_VALUE){
        return 0;
    }

    if(!stn_miner_job_build(server->active_template,
            server->active_template_length,header,&block_length))
    {
        log_line(server,"ERROR active Chain template cannot form Miner JOB.");
        return 0;
    }

    if(!send_all(socket_fd,header,sizeof(header)) ||
       !send_all(
           socket_fd,
           server->active_template+68u,
           block_length))
    {
        client_remove(server,client,"job send failed");
        return 0;
    }

    memcpy(client->sent_job_id,server->active_job_id,32);
    client->has_job=1;

    {
        char job[17];

        hex8(job,server->active_job_id);

        log_line(
            server,
            "CLIENT job sent job=%s... block=%u bytes.",
            job,
            (unsigned)block_length);
    }

    return 1;
}

static void accept_from_listener(
    stn_stratum_server *server,
    const stn_mining_listener *listener)
{
    for(;;){
        stn_stratum_client_session *client=NULL;
        int result=stn_session_acceptor_accept(listener,&client);

        if(result==0){return;}
        if(result<0){
            log_line(server,"WARN accept failed on %s port %u.",
                listener->mode==STN_SERVICE_MODE_SOLO?"Solo":"Pool",
                (unsigned)listener->port);
            return;
        }

        client->next=server->clients;
        server->clients=client;
        server->client_count++;

        log_line(server,"CLIENT connected mode=%s port=%u; clients=%zu.",
            stn_client_mode_is_solo(&client->mode)?"solo":"pool",
            (unsigned)listener->port,
            server->client_count);
    }
}

static void accept_clients(stn_stratum_server *server)
{
    accept_from_listener(server,&server->pool_listener);
    accept_from_listener(server,&server->solo_listener);
}

static uint32_t submit_result_code(stn_rpc_client_code code)
{
    switch(code){
    case STN_RPC_CLIENT_OK:
        return STN_MINER_RESULT_ACCEPTED;

    case STN_RPC_CLIENT_STALE:
        return STN_MINER_RESULT_STALE;

    case STN_RPC_CLIENT_REJECTED:
        return STN_MINER_RESULT_REJECTED;

    case STN_RPC_CLIENT_INVALID:
    case STN_RPC_CLIENT_VERSION_ERROR:
        return STN_MINER_RESULT_PROTOCOL;

    default:
        return STN_MINER_RESULT_PROVIDER;
    }
}

static void client_send_result(
    stn_stratum_server *server,
    stn_stratum_client_session *client,
    stn_rpc_client_code rpc_code)
{
    uint8_t response[STN_MINER_RESULT_SIZE];
    int socket_fd;

    if(client==NULL){return;}

    if(client->socket==STN_INVALID_SOCKET_VALUE){
        return;
    }

    socket_fd=(int)client->socket;

    memset(response,0,sizeof(response));
    memcpy(response,STN_MINER_MAGIC,4);
    response[4]=STN_MINER_VERSION;
    response[5]=STN_MINER_RESULT;
    write32be(response+8,submit_result_code(rpc_code));

    if(!send_all(socket_fd,response,sizeof(response))){
        client_remove(server,client,"result send failed");
    }
}

static void clear_client_jobs(stn_stratum_server *server)
{
    stn_stratum_client_session *client;

    for(client=server->clients;
        client!=NULL;
        client=client->next)
    {
        client->has_job=0;
        client->hashrate=0u;
        memset(
            client->sent_job_id,
            0,
            sizeof(client->sent_job_id));
    }
}

static void clear_work(stn_stratum_server *server)
{
    memset(
        server->active_base,
        0,
        sizeof(server->active_base));

    memset(
        server->active_job_id,
        0,
        sizeof(server->active_job_id));

    server->active_template_length=0u;
    server->have_work=0;

    clear_client_jobs(server);
}

static stn_chain_config_entry *active_chain(
    stn_stratum_server *server)
{
    if(server==NULL ||
       server->chain_config.server_count==0u ||
       server->active_chain_server>=
           server->chain_config.server_count)
    {
        return NULL;
    }

    return &server->chain_config.servers[
        server->active_chain_server];
}

static int chain_transport_select(
    stn_stratum_server *server,
    size_t index)
{
    stn_chain_config_entry *entry;

    if(server==NULL ||
       index>=server->chain_config.server_count)
    {
        return 0;
    }

    clear_work(server);
    server->chain_available=0;

    stn_rpc_linux_close(&server->chain_transport);

    server->active_chain_server=index;
    entry=&server->chain_config.servers[index];

    stn_rpc_linux_init(
        &server->chain_transport,
        entry->host,
        entry->port);

    server->chain_transport.idle=telemetry_idle;
    server->chain_transport.idle_user=server;

    stn_rpc_client_init(
        &server->chain_rpc,
        &server->chain_transport,
        stn_rpc_linux_exchange);

    log_line(
        server,
        "CHAIN target %s:%u.",
        entry->host,
        (unsigned)entry->port);

    return 1;
}

static int chain_transport_next(
    stn_stratum_server *server)
{
    size_t next;

    if(server==NULL ||
       server->chain_config.server_count==0u)
    {
        return 0;
    }

    next=server->active_chain_server+1u;

    if(next>=server->chain_config.server_count){
        next=0u;
    }

    return chain_transport_select(server,next);
}

static void process_submission(
    stn_stratum_server *server,
    stn_stratum_client_session *client)
{
    const uint8_t *p;
    uint8_t *payload;
    uint8_t response[80];
    size_t response_length=0u;
    uint32_t block_length;
    uint64_t nonce;
    stn_rpc_client_code code;
    stn_share_class share_class;
    uint8_t share_hash[32];

    if(client==NULL){return;}
    if(!client->address_registered){
        log_line(server,"CLIENT SUBMIT before ADDRESS.");
        client_remove(server,client,"ADDRESS required");
        return;
    }

    p=client->rx;

    if(memcmp(p,STN_MINER_MAGIC,4)!=0 ||
       p[4]!=STN_MINER_VERSION ||
       p[5]!=STN_MINER_SUBMIT ||
       p[6]!=0 ||
       p[7]!=0)
    {
        log_line(
            server,
            "CLIENT invalid submit frame.");

        client_send_result(
            server,
            client,
            STN_RPC_CLIENT_INVALID);

        return;
    }

    if(!server->have_work ||
       memcmp(
           p+8,
           server->active_job_id,
           32)!=0)
    {
        log_line(
            server,
            "CLIENT stale job submission.");

        client_send_result(
            server,
            client,
            STN_RPC_CLIENT_STALE);

        return;
    }

    if(server->active_template_length<68u){
        log_line(
            server,
            "ERROR active Chain template invalid during submit.");

        client_send_result(
            server,
            client,
            STN_RPC_CLIENT_INVALID);

        return;
    }

    block_length=read32be(
        server->active_template+64);

    if((size_t)block_length!=
           server->active_template_length-68u ||
       block_length<STN_MINER_CHAIN_HEADER_SIZE)
    {
        log_line(
            server,
            "ERROR active Chain template invalid during submit.");

        client_send_result(
            server,
            client,
            STN_RPC_CLIENT_INVALID);

        return;
    }

    nonce=read64be(p+40);

    {
        uint8_t share_target[32];
        const uint8_t *chain_target=
            server->active_template+68u+STN_MINER_CHAIN_TARGET_OFFSET;

        if(!share_target_from_chain(chain_target,share_target)){
            log_line(server,"ERROR active Chain target invalid during submit.");
            client_send_result(server,client,STN_RPC_CLIENT_INVALID);
            return;
        }

        share_class=stn_share_classify(
            server->active_template+68u,
            block_length,
            nonce,
            share_target,
            chain_target,
            share_hash);

        if(share_class==STN_SHARE_INVALID){
            log_line(server,"CLIENT submit rejected: proof exceeds Share Target.");
            client_send_result(server,client,STN_RPC_CLIENT_REJECTED);
            return;
        }

        if(share_class==STN_SHARE_QUALIFYING){
            uint8_t share_payload[STN_RPC_CLIENT_SHARE_SIZE];
            uint8_t share_id[32];
            stn_rpc_client_code share_code;
            char share_hex[17];

            memcpy(share_payload,server->active_job_id,32u);
            memcpy(share_payload+32u,client->address,STN_MINER_ADDRESS_LENGTH);
            {
                size_t i;
                uint64_t value=nonce;
                for(i=0u;i<8u;i++){
                    share_payload[108u-i]=(uint8_t)(value&0xffu);
                    value>>=8;
                }
            }
            memcpy(share_payload+STN_RPC_CLIENT_SHARE_PREFIX_SIZE,
                server->active_template+68u,
                STN_MINER_CHAIN_HEADER_SIZE);

            share_code=stn_rpc_client_submit_share(
                &server->chain_rpc,
                share_payload,
                sizeof(share_payload),
                share_id);

            if(share_code==STN_RPC_CLIENT_OK){
                hex8(share_hex,share_id);
                log_line(server,
                    "CLIENT qualifying share nonce=%llu Chain verified share=%s....",
                    (unsigned long long)nonce,share_hex);
            }else{
                log_line(server,
                    "CLIENT qualifying share nonce=%llu Chain result=%d.",
                    (unsigned long long)nonce,(int)share_code);
            }

            client_send_result(server,client,share_code);
            /*
             * An accepted qualifying share does not consume the active Work ID.
             * Other miners, and this miner, may continue producing independent
             * qualifying evidence against the same immutable Chain work.
             * Clear only when Chain says the work/session can no longer be used.
             */
            if(share_code==STN_RPC_CLIENT_STALE ||
               share_code==STN_RPC_CLIENT_TRANSPORT ||
               share_code==STN_RPC_CLIENT_UNAVAILABLE ||
               share_code==STN_RPC_CLIENT_PROVIDER ||
               share_code==STN_RPC_CLIENT_INVALID){
                clear_work(server);
            }
            return;
        }
    }

    payload=(uint8_t *)malloc(
        server->active_template_length+STN_MINER_ADDRESS_LENGTH);
    if(payload==NULL){
        log_line(server,
            "ERROR submission scratch allocation failed: %zu bytes.",
            server->active_template_length);
        client_send_result(server,client,STN_RPC_CLIENT_PROVIDER);
        return;
    }
    memcpy(payload,server->active_template,68u);
    memcpy(payload+68u,client->address,STN_MINER_ADDRESS_LENGTH);
    memcpy(payload+68u+STN_MINER_ADDRESS_LENGTH,
        server->active_template+68u,
        server->active_template_length-68u);

    {
        uint8_t *nonce_field=
            payload+68u+STN_MINER_ADDRESS_LENGTH+
            STN_MINER_CHAIN_NONCE_OFFSET;
        size_t i;
        uint64_t value=nonce;

        for(i=0u;i<8u;i++){
            nonce_field[7u-i]=(uint8_t)(value&0xffu);
            value>>=8;
        }
    }

    code=stn_rpc_client_submit_work(
        &server->chain_rpc,
        payload,
        server->active_template_length+STN_MINER_ADDRESS_LENGTH,
        response,
        sizeof(response),
        &response_length);

    free(payload);

    log_line(
        server,
        "CLIENT submit nonce=%llu Chain result=%d response=%zu.",
        (unsigned long long)nonce,
        (int)code,
        response_length);

    client_send_result(server,client,code);

    if(code==STN_RPC_CLIENT_OK ||
       code==STN_RPC_CLIENT_STALE ||
       code==STN_RPC_CLIENT_TRANSPORT ||
       code==STN_RPC_CLIENT_UNAVAILABLE ||
       code==STN_RPC_CLIENT_PROVIDER ||
       code==STN_RPC_CLIENT_INVALID)
    {
        clear_work(server);
    }

    if(code==STN_RPC_CLIENT_TRANSPORT ||
       code==STN_RPC_CLIENT_UNAVAILABLE)
    {
        log_line(
            server,
            "CHAIN RPC endpoint unavailable after submission; "
            "selecting next configured server.");

        (void)chain_transport_next(server);
    }
}

static int address_valid(const uint8_t *p)
{
    size_t i;
    if(p==NULL){return 0;}
    if(memcmp(p,STN_MINER_MAGIC,4)!=0 ||
       p[4]!=STN_MINER_VERSION ||
       p[5]!=STN_MINER_ADDRESS ||
       p[6]!=0 || p[7]!=0){return 0;}
    if(memcmp(p+8,"stn0_",5)!=0){return 0;}
    for(i=13u;i<8u+STN_MINER_ADDRESS_LENGTH;i++){
        if(!((p[i]>=(uint8_t)'0' && p[i]<=(uint8_t)'9') ||
             (p[i]>=(uint8_t)'a' && p[i]<=(uint8_t)'f'))){return 0;}
    }
    return 1;
}

static void process_address(
    stn_stratum_server *server,
    stn_stratum_client_session *client)
{
    if(client==NULL){return;}
    if(client->address_registered){
        log_line(server,"CLIENT duplicate ADDRESS rejected.");
        client_remove(server,client,"duplicate ADDRESS");
        return;
    }
    if(!address_valid(client->rx)){
        log_line(server,"CLIENT invalid ADDRESS frame.");
        client_remove(server,client,"invalid ADDRESS");
        return;
    }
    memcpy(client->address,client->rx+8,STN_MINER_ADDRESS_LENGTH);
    client->address[STN_MINER_ADDRESS_LENGTH]='\0';
    client->address_registered=1;
    log_line(server,"CLIENT address registered: %s.",client->address);
    if(server->have_work){(void)client_send_job(server,client);}
}

static void process_hash_progress(
    stn_stratum_server *server,
    stn_stratum_client_session *client)
{
    const uint8_t *p;
    uint64_t hashes;
    uint64_t elapsed_ms;
    uint64_t quotient;
    uint64_t remainder;
    uint64_t rate;

    if(server==NULL || client==NULL){return;}
    if(!client->address_registered){
        log_line(server,"CLIENT HASH_PROGRESS before ADDRESS.");
        client_remove(server,client,"ADDRESS required");
        return;
    }

    p=client->rx;

    if(memcmp(p,STN_MINER_MAGIC,4)!=0 ||
       p[4]!=STN_MINER_VERSION ||
       p[5]!=STN_MINER_HASH_PROGRESS ||
       p[6]!=0 ||
       p[7]!=0)
    {
        return;
    }

    if(!server->have_work ||
       !client->has_job ||
       memcmp(p+8,server->active_job_id,32)!=0 ||
       memcmp(p+8,client->sent_job_id,32)!=0)
    {
        client->hashrate=0u;
        return;
    }

    hashes=read64be(p+40);
    elapsed_ms=read64be(p+48);

    if(elapsed_ms==0u){
        client->hashrate=0u;
        return;
    }

    quotient=hashes/elapsed_ms;
    remainder=hashes%elapsed_ms;

    if(quotient>UINT64_MAX/1000u){
        rate=UINT64_MAX;
    }
    else{
        rate=quotient*1000u;

        {
            uint64_t extra;

            if(remainder<=UINT64_MAX/1000u){
                extra=(remainder*1000u)/elapsed_ms;
            }
            else{
                uint64_t scaled_divisor=(elapsed_ms/1000u)+1u;
                extra=remainder/scaled_divisor;
                if(extra>999u){extra=999u;}
            }

            if(UINT64_MAX-rate<extra){
                rate=UINT64_MAX;
            }
            else{
                rate+=extra;
            }
        }
    }

    client->hashrate=rate;
}

static void service_client(
    stn_stratum_server *server,
    stn_stratum_client_session *client)
{
    int socket_fd;

    if(client==NULL){return;}

    if(client->socket==STN_INVALID_SOCKET_VALUE){
        return;
    }

    socket_fd=(int)client->socket;

    for(;;){
        size_t frame_size;

        while(client->rx_used<8u){
            ssize_t got=recv(
                socket_fd,
                client->rx+client->rx_used,
                8u-client->rx_used,
                0);

            if(got>0){
                client->rx_used+=(size_t)got;
                continue;
            }

            if(got==0){
                client_remove(
                    server,
                    client,
                    "peer closed");
                return;
            }

            if(errno==EINTR){
                continue;
            }

            if(errno==EAGAIN ||
               errno==EWOULDBLOCK)
            {
                return;
            }

            client_remove(
                server,
                client,
                "receive failed");
            return;
        }

        if(memcmp(
               client->rx,
               STN_MINER_MAGIC,
               4)!=0 ||
           client->rx[4]!=STN_MINER_VERSION ||
           client->rx[6]!=0 ||
           client->rx[7]!=0)
        {
            client_remove(
                server,
                client,
                "invalid frame header");
            return;
        }

        if(client->rx[5]==STN_MINER_ADDRESS){
            frame_size=STN_MINER_ADDRESS_FRAME_SIZE;
        }
        else if(client->rx[5]==STN_MINER_SUBMIT){
            frame_size=STN_MINER_SUBMIT_SIZE;
        }
        else if(client->rx[5]==STN_MINER_HASH_PROGRESS){
            frame_size=STN_MINER_HASH_PROGRESS_SIZE;
        }
        else{
            client_remove(
                server,
                client,
                "unsupported frame type");
            return;
        }

        while(client->rx_used<frame_size){
            ssize_t got=recv(
                socket_fd,
                client->rx+client->rx_used,
                frame_size-client->rx_used,
                0);

            if(got>0){
                client->rx_used+=(size_t)got;
                continue;
            }

            if(got==0){
                client_remove(
                    server,
                    client,
                    "peer closed");
                return;
            }

            if(errno==EINTR){
                continue;
            }

            if(errno==EAGAIN ||
               errno==EWOULDBLOCK)
            {
                return;
            }

            client_remove(
                server,
                client,
                "receive failed");
            return;
        }

        if(client->rx[5]==STN_MINER_ADDRESS){
            process_address(
                server,
                client);
        }
        else if(client->rx[5]==STN_MINER_SUBMIT){
            process_submission(
                server,
                client);
        }
        else{
            process_hash_progress(
                server,
                client);
        }

        {
            stn_stratum_client_session *probe;
            int still_connected=0;

            for(probe=server->clients;
                probe!=NULL;
                probe=probe->next)
            {
                if(probe==client){
                    still_connected=1;
                    break;
                }
            }

            if(!still_connected){
                return;
            }
        }

        client->rx_used=0u;
        /* One complete frame per pass: a busy miner must yield to peers
         * and the Chain work poll instead of draining an unbounded stream. */
        return;
    }
}

static void service_clients(stn_stratum_server *server)
{
    stn_stratum_client_session *client=
        server->clients;

    while(client!=NULL){
        stn_stratum_client_session *next=
            client->next;

        service_client(server,client);
        client=next;
    }
}

static void send_pending_jobs(stn_stratum_server *server)
{
    stn_stratum_client_session *client=
        server->clients;

    while(client!=NULL){
        stn_stratum_client_session *next=
            client->next;

        if(server->have_work &&
           client->address_registered &&
           (!client->has_job ||
            memcmp(
                client->sent_job_id,
                server->active_job_id,
                32)!=0))
        {
            (void)client_send_job(
                server,
                client);
        }

        client=next;
    }
}

static void refresh_work(stn_stratum_server *server)
{
    uint8_t *payload=server->active_template;
    size_t written=0u;
    uint32_t block_length;
    stn_rpc_client_code code;
    stn_chain_config_entry *entry;
    int changed;

    code=stn_rpc_client_mining_template(
        &server->chain_rpc,
        payload,
        sizeof(server->active_template),
        &written);

    server->chain_poll_count++;

    if(code!=STN_RPC_CLIENT_OK){
        if(code==STN_RPC_CLIENT_STALE){
            log_line(
                server,
                "CHAIN work stale; invalidating current job and refreshing.");

            server->chain_available=1;
            clear_work(server);
            return;
        }

        if(code==STN_RPC_CLIENT_CAPACITY){
            log_line(
                server,
                "ERROR CHAIN RPC capacity failure code=%d; "
                "connection remains available; retrying.",
                (int)code);

            server->chain_available=1;
            clear_work(server);
            return;
        }

        if(code==STN_RPC_CLIENT_UNAVAILABLE){
            if(!server->chain_available || server->have_work ||
               (server->chain_poll_count%STN_STRATUM_HEARTBEAT_CHAIN_POLLS)==1u){
                log_line(server,
                    "CHAIN RPC reachable; mining template unavailable code=5; "
                    "retaining server and retrying.");
            }
            server->chain_available=1;
            /*
             * Chain is reachable but has no replacement template yet.
             * Retain the current immutable job until Chain supplies new work
             * or explicitly reports it stale.
             */
            return;
        }

        if(code==STN_RPC_CLIENT_TRANSPORT)
        {
            if(server->chain_available){
                log_line(
                    server,
                    "CHAIN RPC connection unavailable code=%d; "
                    "work invalidated.",
                    (int)code);
            }
            else if(
                (server->chain_poll_count%
                 STN_STRATUM_HEARTBEAT_CHAIN_POLLS)==1u)
            {
                log_line(
                    server,
                    "CHAIN RPC connection still unavailable code=%d; "
                    "selecting next configured server.",
                    (int)code);
            }

            server->chain_available=0;
            clear_work(server);

            (void)chain_transport_next(server);
            return;
        }

        log_line(
            server,
            "ERROR CHAIN mining-template RPC rejected code=%d; retrying.",
            (int)code);

        server->chain_available=1;
        clear_work(server);
        return;
    }

    if(written<68u){
        log_line(
            server,
            "ERROR CHAIN mining template malformed: %zu bytes.",
            written);

        server->chain_available=0;
        clear_work(server);
        return;
    }

    block_length=read32be(payload+64);

    if((size_t)block_length!=written-68u){
        log_line(
            server,
            "ERROR CHAIN template length mismatch frame=%zu block=%u.",
            written,
            (unsigned)block_length);

        server->chain_available=0;
        clear_work(server);
        return;
    }

    if(!server->chain_available){
        entry=active_chain(server);

        if(entry!=NULL){
            log_line(
                server,
                "CHAIN RPC connected %s:%u.",
                entry->host,
                (unsigned)entry->port);
        }
    }

    server->chain_available=1;

    changed=
        !server->have_work ||
        memcmp(
            server->active_base,
            payload,
            32)!=0 ||
        memcmp(
            server->active_job_id,
            payload+32,
            32)!=0;

    server->active_template_length=written;

    if(changed){
        char base[17];
        char job[17];

        memcpy(
            server->active_base,
            payload,
            32);

        memcpy(
            server->active_job_id,
            payload+32,
            32);

        server->have_work=1;

        hex8(base,server->active_base);
        hex8(job,server->active_job_id);

        log_line(
            server,
            "WORK new base=%s... job=%s... block=%u bytes.",
            base,
            job,
            (unsigned)block_length);

        send_pending_jobs(server);
        return;
    }

    if((server->chain_poll_count%
        STN_STRATUM_HEARTBEAT_CHAIN_POLLS)==0u)
    {
        char base[17];
        char job[17];

        hex8(base,server->active_base);
        hex8(job,server->active_job_id);

        log_line(
            server,
            "HEARTBEAT running chain=connected work=active clients=%zu "
            "base=%s... job=%s....",
            server->client_count,
            base,
            job);
    }
}

static void sleep_poll_interval(void)
{
    struct timespec request;
    struct timespec remaining;

    request.tv_sec=
        (time_t)(STN_STRATUM_POLL_MS/1000u);

    request.tv_nsec=
        (long)((STN_STRATUM_POLL_MS%1000u)*1000000u);

    while(nanosleep(&request,&remaining)!=0){
        if(errno!=EINTR){
            return;
        }

        request=remaining;
    }
}

void stn_stratum_server_init(stn_stratum_server *server)
{
    if(server==NULL){return;}

    memset(server,0,sizeof(*server));

    stn_mining_listener_init(
        &server->pool_listener,
        STN_SERVICE_MODE_POOL);

    stn_mining_listener_init(
        &server->solo_listener,
        STN_SERVICE_MODE_SOLO);

    stn_telemetry_service_init(
        &server->telemetry,
        STN_STRATUM_TELEMETRY_PORT);

    server->started_at=time(NULL);
    server->running=1;
}

int stn_stratum_server_run(stn_stratum_server *server)
{
    stn_chain_config_entry *entry;

    if(server==NULL){return 0;}

    if(!log_open(server)){
        fprintf(
            stderr,
            "Unable to open local log: %s\n",
            STN_STRATUM_LOG_PATH);

        return 0;
    }

    log_line(
        server,
        "START STN-Stratum development server.");

    log_line(
        server,
        "LOG %s.",
        STN_STRATUM_LOG_PATH);

    log_line(
        server,
        "CHAIN config %s.",
        STN_STRATUM_CHAIN_CONFIG);

    if(!stn_chain_config_load(
        &server->chain_config,
        STN_STRATUM_CHAIN_CONFIG))
    {
        log_line(
            server,
            "ERROR unable to load Chain configuration: %s.",
            STN_STRATUM_CHAIN_CONFIG);

        log_line(
            server,
            "STOP startup failed.");

        return 0;
    }

    if(server->chain_config.server_count==0u){
        log_line(
            server,
            "ERROR Chain configuration contains no servers.");

        log_line(
            server,
            "STOP startup failed.");

        return 0;
    }

    server->active_chain_server=0u;

    if(!chain_transport_select(server,0u)){
        log_line(
            server,
            "ERROR unable to select configured Chain server.");

        log_line(
            server,
            "STOP startup failed.");

        return 0;
    }

    entry=active_chain(server);

    if(entry==NULL){
        log_line(
            server,
            "ERROR configured Chain server unavailable.");

        log_line(
            server,
            "STOP startup failed.");

        return 0;
    }

    log_line(
        server,
        "CHAIN servers loaded=%zu.",
        server->chain_config.server_count);

    if(!listeners_open(server)){
        log_line(
            server,
            "STOP startup failed.");

        return 0;
    }

    if(!stn_telemetry_service_open(&server->telemetry)){
        log_line(
            server,
            "ERROR unable to open telemetry listener on port %u.",
            (unsigned)STN_STRATUM_TELEMETRY_PORT);
        listeners_close(server);
        log_line(server,"STOP startup failed.");
        return 0;
    }

    log_line(server,"TELEMETRY 0.0.0.0:%u ready.",
        (unsigned)STN_STRATUM_TELEMETRY_PORT);

    while(server->running){
        accept_clients(server);
        service_clients(server);
        telemetry_tick(server);

        if((server->loop_count%
            STN_STRATUM_CHAIN_POLL_TICKS)==0u)
        {
            refresh_work(server);
        }

        send_pending_jobs(server);

        server->loop_count++;
        sleep_poll_interval();
    }

    log_line(
        server,
        "STOP requested.");

    return 1;
}

void stn_stratum_server_stop(stn_stratum_server *server)
{
    if(server!=NULL){
        server->running=0;
    }
}

void stn_stratum_server_close(stn_stratum_server *server)
{
    if(server==NULL){return;}

    while(server->clients!=NULL){
        client_remove(
            server,
            server->clients,
            NULL);
    }

    stn_telemetry_service_close(&server->telemetry);
    listeners_close(server);

    stn_rpc_linux_close(
        &server->chain_transport);

    clear_work(server);
    server->chain_available=0;

    stn_chain_config_close(
        &server->chain_config);

    log_line(
        server,
        "STOP complete.");

    log_close(server);
}

#else

void stn_stratum_server_init(stn_stratum_server *server)
{
    (void)server;
}

int stn_stratum_server_run(stn_stratum_server *server)
{
    (void)server;
    return 0;
}

void stn_stratum_server_stop(stn_stratum_server *server)
{
    (void)server;
}

void stn_stratum_server_close(stn_stratum_server *server)
{
    (void)server;
}

#endif
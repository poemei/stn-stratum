/* Copyright (c) 2026 STN-Labz. All rights reserved. */
#include "stn_stratum_server.h"

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

struct stn_stratum_client_session {
    uintptr_t socket;
    uint8_t rx[STN_MINER_HASH_PROGRESS_SIZE];
    size_t rx_used;
    uint8_t sent_job_id[32];
    uint64_t hashrate;
    int has_job;
    struct stn_stratum_client_session *next;
};

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

    if((SOCKET)client->socket!=INVALID_SOCKET){
        closesocket((SOCKET)client->socket);
        client->socket=STN_INVALID_SOCKET_VALUE;
    }

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

    free(client);
}

static int listener_open(stn_stratum_server *server)
{
    SOCKET s;
    struct sockaddr_in addr;
    u_long nonblocking=1u;

    if(!winsock_open(server)){
        log_line(server,"ERROR Winsock initialization failed.");
        return 0;
    }

    s=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
    if(s==INVALID_SOCKET){
        log_line(server,"ERROR listener socket failed: WSA=%d.",WSAGetLastError());
        return 0;
    }

    memset(&addr,0,sizeof(addr));
    addr.sin_family=AF_INET;
    addr.sin_addr.s_addr=htonl(INADDR_ANY);
    addr.sin_port=htons(STN_STRATUM_DEFAULT_PORT);

    if(bind(s,(const struct sockaddr *)&addr,sizeof(addr))!=0){
        log_line(server,"ERROR bind 0.0.0.0:%u failed: WSA=%d.",
            (unsigned)STN_STRATUM_DEFAULT_PORT,WSAGetLastError());
        closesocket(s);
        return 0;
    }

    if(listen(s,SOMAXCONN)!=0){
        log_line(server,"ERROR listen failed: WSA=%d.",WSAGetLastError());
        closesocket(s);
        return 0;
    }

    if(ioctlsocket(s,FIONBIO,&nonblocking)!=0){
        log_line(server,"ERROR listener nonblocking failed: WSA=%d.",
            WSAGetLastError());
        closesocket(s);
        return 0;
    }

    server->listen_socket=(uintptr_t)s;
    log_line(server,"LISTEN 0.0.0.0:%u ready.",
        (unsigned)STN_STRATUM_DEFAULT_PORT);
    return 1;
}

static void listener_close(stn_stratum_server *server)
{
    SOCKET s=(SOCKET)server->listen_socket;

    if(s!=INVALID_SOCKET){
        closesocket(s);
        server->listen_socket=STN_INVALID_SOCKET_VALUE;
        log_line(server,"LISTEN port %u closed.",
            (unsigned)STN_STRATUM_DEFAULT_PORT);
    }
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

    block_length=(uint32_t)(server->active_template_length-68u);

    memset(header,0,sizeof(header));
    memcpy(header,STN_MINER_MAGIC,4);
    header[4]=STN_MINER_VERSION;
    header[5]=STN_MINER_JOB;
    memcpy(header+8,server->active_job_id,32);

    if(block_length<STN_MINER_CHAIN_HEADER_SIZE){
        log_line(server,
            "ERROR active Chain template block too small: %u bytes.",
            (unsigned)block_length);
        return 0;
    }

    memcpy(
        header+40,
        server->active_template+68u+STN_MINER_CHAIN_TARGET_OFFSET,
        32u);

    write32be(header+72,block_length);
    memcpy(
        header+76,
        server->active_template+68u+STN_MINER_CHAIN_NONCE_OFFSET,
        8u);

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

static void accept_clients(stn_stratum_server *server)
{
    SOCKET listener=(SOCKET)server->listen_socket;
    SOCKET socket;
    struct sockaddr_storage peer;
    int peer_length;
    u_long nonblocking;

    for(;;){
        stn_stratum_client_session *client;

        peer_length=(int)sizeof(peer);
        socket=accept(listener,(struct sockaddr *)&peer,&peer_length);

        if(socket==INVALID_SOCKET){
            if(WSAGetLastError()==WSAEWOULDBLOCK){return;}
            log_line(server,"WARN accept failed: WSA=%d.",WSAGetLastError());
            return;
        }

        nonblocking=1u;
        if(ioctlsocket(socket,FIONBIO,&nonblocking)!=0){
            log_line(server,"CLIENT nonblocking setup failed: WSA=%d.",
                WSAGetLastError());
            closesocket(socket);
            continue;
        }

        client=(stn_stratum_client_session *)calloc(1u,sizeof(*client));
        if(client==NULL){
            log_line(server,
                "CLIENT rejected: host memory unavailable.");
            closesocket(socket);
            continue;
        }

        client->socket=(uintptr_t)socket;
        client->next=server->clients;
        server->clients=client;
        server->client_count++;

        log_line(server,"CLIENT connected; clients=%zu.",server->client_count);

        if(server->have_work){
            (void)client_send_job(server,client);
        }
    }
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

    if(client==NULL){return;}
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

    payload=(uint8_t *)malloc(server->active_template_length);
    if(payload==NULL){
        log_line(server,
            "ERROR submission scratch allocation failed: %zu bytes.",
            server->active_template_length);
        client_send_result(server,client,STN_RPC_CLIENT_PROVIDER);
        return;
    }

    memcpy(payload,server->active_template,server->active_template_length);

    nonce=read64be(p+40);

    {
        uint8_t *nonce_field=
            payload+68u+STN_MINER_CHAIN_NONCE_OFFSET;
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
        server->active_template_length,
        response,
        sizeof(response),
        &response_length);

    free(payload);

    log_line(server,"CLIENT submit nonce=%llu Chain result=%d response=%zu.",
        (unsigned long long)nonce,(int)code,response_length);

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
        log_line(server,
            "CHAIN RPC endpoint unavailable after submission; "
            "selecting next configured server.");

        (void)chain_transport_next(server);
    }
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

        if(frame_size==STN_MINER_SUBMIT_SIZE){
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
            clear_work(server);
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
    server->listen_socket=STN_INVALID_SOCKET_VALUE;
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

    if(!listener_open(server)){
        log_line(server,"STOP startup failed.");
        return 0;
    }

    while(server->running){
        accept_clients(server);
        service_clients(server);

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

    listener_close(server);
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
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define STN_INVALID_SOCKET_VALUE ((uintptr_t)-1)

struct stn_stratum_client_session {
    uintptr_t socket;
    uint8_t rx[STN_MINER_HASH_PROGRESS_SIZE];
    size_t rx_used;
    uint8_t sent_job_id[32];
    uint64_t hashrate;
    int has_job;
    struct stn_stratum_client_session *next;
};

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

    if(client->socket!=STN_INVALID_SOCKET_VALUE){
        (void)close((int)client->socket);
        client->socket=STN_INVALID_SOCKET_VALUE;
    }

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

    free(client);
}

static void hex32(char out[65],const uint8_t value[32])
{
    static const char table[]="0123456789abcdef";
    size_t i;

    for(i=0u;i<32u;i++){
        out[i*2u]=table[(value[i]>>4)&0x0fu];
        out[i*2u+1u]=table[value[i]&0x0fu];
    }

    out[64]='\0';
}

static int telemetry_open(stn_stratum_server *server)
{
    int socket_fd;
    int flags;
    int reuse=1;
    struct sockaddr_in addr;

    socket_fd=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
    if(socket_fd<0){return 0;}

    if(setsockopt(socket_fd,SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof(reuse))!=0){
        (void)close(socket_fd);
        return 0;
    }

    memset(&addr,0,sizeof(addr));
    addr.sin_family=AF_INET;
    addr.sin_addr.s_addr=htonl(INADDR_ANY);
    addr.sin_port=htons(STN_STRATUM_TELEMETRY_PORT);

    if(bind(socket_fd,(const struct sockaddr *)&addr,sizeof(addr))!=0 ||
       listen(socket_fd,SOMAXCONN)!=0)
    {
        (void)close(socket_fd);
        return 0;
    }

    flags=fcntl(socket_fd,F_GETFL,0);
    if(flags<0 || fcntl(socket_fd,F_SETFL,flags|O_NONBLOCK)!=0){
        (void)close(socket_fd);
        return 0;
    }

    server->telemetry_socket=(uintptr_t)socket_fd;
    log_line(server,"TELEMETRY 0.0.0.0:%u ready.",(unsigned)STN_STRATUM_TELEMETRY_PORT);
    return 1;
}

static void telemetry_close(stn_stratum_server *server)
{
    if(server->telemetry_socket!=STN_INVALID_SOCKET_VALUE){
        (void)close((int)server->telemetry_socket);
        server->telemetry_socket=STN_INVALID_SOCKET_VALUE;
        log_line(server,"TELEMETRY port %u closed.",(unsigned)STN_STRATUM_TELEMETRY_PORT);
    }
}

static stn_chain_config_entry *active_chain(
    stn_stratum_server *server);

static void telemetry_service(stn_stratum_server *server)
{
    int client;
    char request[512];
    char body[1024];
    char response[1400];
    char job[65];
    char base[65];
    char target[65];
    const char *chain_host="";
    unsigned chain_port=0u;
    unsigned long long uptime=0u;
    uint64_t hashrate=0u;
    ssize_t got;
    int body_length;
    int response_length;
    time_t now;
    stn_chain_config_entry *entry;

    client=accept((int)server->telemetry_socket,NULL,NULL);
    if(client<0){
        if(errno!=EAGAIN && errno!=EWOULDBLOCK && errno!=EINTR){
            log_line(server,"WARN telemetry accept failed: errno=%d.",errno);
        }
        return;
    }

    got=recv(client,request,sizeof(request)-1u,0);
    if(got<=0){(void)close(client);return;}
    request[(size_t)got]='\0';

    if(strncmp(request,"GET /status ",12)!=0 && strncmp(request,"GET / ",6)!=0){
        static const char not_found[]="HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
        (void)send_all(client,(const uint8_t *)not_found,sizeof(not_found)-1u);
        (void)close(client);
        return;
    }

    memset(job,'0',64u); job[64]='\0';
    memset(base,'0',64u); base[64]='\0';
    memset(target,'0',64u); target[64]='\0';

    if(server->have_work){
        hex32(job,server->active_job_id);
        hex32(base,server->active_base);
        if(server->active_template_length>=68u+STN_MINER_CHAIN_TARGET_OFFSET+32u){
            hex32(target,server->active_template+68u+STN_MINER_CHAIN_TARGET_OFFSET);
        }
    }

    entry=active_chain(server);
    if(entry!=NULL){chain_host=entry->host;chain_port=(unsigned)entry->port;}

    now=time(NULL);
    if(server->started_at!=(time_t)0 && now>=server->started_at){
        uptime=(unsigned long long)(now-server->started_at);
    }

    {
        stn_stratum_client_session *miner;

        for(miner=server->clients;miner!=NULL;miner=miner->next){
            if(UINT64_MAX-hashrate<miner->hashrate){
                hashrate=UINT64_MAX;
                break;
            }

            hashrate+=miner->hashrate;
        }
    }

    body_length=snprintf(body,sizeof(body),
        "{\"service\":\"STN-Stratum\",\"status\":\"running\",\"chain_connected\":%s,\"chain_host\":\"%s\",\"chain_port\":%u,\"work_available\":%s,\"miners\":%zu,\"hashrate\":%llu,\"job_id\":\"%s\",\"base_id\":\"%s\",\"target\":\"%s\",\"uptime_seconds\":%llu}\n",
        server->chain_available?"true":"false",chain_host,chain_port,
        server->have_work?"true":"false",server->client_count,
        (unsigned long long)hashrate,job,base,target,uptime);

    if(body_length<0 || (size_t)body_length>=sizeof(body)){(void)close(client);return;}

    response_length=snprintf(response,sizeof(response),
        "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: %d\r\nCache-Control: no-store\r\nConnection: close\r\n\r\n%s",
        body_length,body);

    if(response_length>0 && (size_t)response_length<sizeof(response)){
        (void)send_all(client,(const uint8_t *)response,(size_t)response_length);
    }
    (void)close(client);
}

static int listener_open(stn_stratum_server *server)
{
    int socket_fd;
    int flags;
    int reuse=1;
    struct sockaddr_in addr;

    socket_fd=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);

    if(socket_fd<0){
        log_line(
            server,
            "ERROR listener socket failed: errno=%d.",
            errno);
        return 0;
    }

    if(setsockopt(
        socket_fd,
        SOL_SOCKET,
        SO_REUSEADDR,
        &reuse,
        sizeof(reuse))!=0)
    {
        log_line(
            server,
            "ERROR listener reuse setup failed: errno=%d.",
            errno);
        (void)close(socket_fd);
        return 0;
    }

    memset(&addr,0,sizeof(addr));
    addr.sin_family=AF_INET;
    addr.sin_addr.s_addr=htonl(INADDR_ANY);
    addr.sin_port=htons(STN_STRATUM_DEFAULT_PORT);

    if(bind(
        socket_fd,
        (const struct sockaddr *)&addr,
        sizeof(addr))!=0)
    {
        log_line(
            server,
            "ERROR bind 0.0.0.0:%u failed: errno=%d.",
            (unsigned)STN_STRATUM_DEFAULT_PORT,
            errno);
        (void)close(socket_fd);
        return 0;
    }

    if(listen(socket_fd,SOMAXCONN)!=0){
        log_line(
            server,
            "ERROR listen failed: errno=%d.",
            errno);
        (void)close(socket_fd);
        return 0;
    }

    flags=fcntl(socket_fd,F_GETFL,0);

    if(flags<0 ||
       fcntl(socket_fd,F_SETFL,flags|O_NONBLOCK)!=0)
    {
        log_line(
            server,
            "ERROR listener nonblocking failed: errno=%d.",
            errno);
        (void)close(socket_fd);
        return 0;
    }

    server->listen_socket=(uintptr_t)socket_fd;

    log_line(
        server,
        "LISTEN 0.0.0.0:%u ready.",
        (unsigned)STN_STRATUM_DEFAULT_PORT);

    return 1;
}

static void listener_close(stn_stratum_server *server)
{
    if(server->listen_socket!=STN_INVALID_SOCKET_VALUE){
        (void)close((int)server->listen_socket);
        server->listen_socket=STN_INVALID_SOCKET_VALUE;

        log_line(
            server,
            "LISTEN port %u closed.",
            (unsigned)STN_STRATUM_DEFAULT_PORT);
    }
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

    block_length=(uint32_t)(server->active_template_length-68u);

    memset(header,0,sizeof(header));
    memcpy(header,STN_MINER_MAGIC,4);
    header[4]=STN_MINER_VERSION;
    header[5]=STN_MINER_JOB;
    memcpy(header+8,server->active_job_id,32);

    if(block_length<STN_MINER_CHAIN_HEADER_SIZE){
        log_line(
            server,
            "ERROR active Chain template block too small: %u bytes.",
            (unsigned)block_length);
        return 0;
    }

    memcpy(
        header+40,
        server->active_template+68u+STN_MINER_CHAIN_TARGET_OFFSET,
        32u);

    write32be(header+72,block_length);

    memcpy(
        header+76,
        server->active_template+68u+STN_MINER_CHAIN_NONCE_OFFSET,
        8u);

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

static void accept_clients(stn_stratum_server *server)
{
    int listener=(int)server->listen_socket;

    for(;;){
        int socket_fd;
        int flags;
        struct sockaddr_storage peer;
        socklen_t peer_length;
        stn_stratum_client_session *client;

        peer_length=(socklen_t)sizeof(peer);

        socket_fd=accept(
            listener,
            (struct sockaddr *)&peer,
            &peer_length);

        if(socket_fd<0){
            if(errno==EAGAIN || errno==EWOULDBLOCK){
                return;
            }

            if(errno==EINTR){
                continue;
            }

            log_line(
                server,
                "WARN accept failed: errno=%d.",
                errno);
            return;
        }

        flags=fcntl(socket_fd,F_GETFL,0);

        if(flags<0 ||
           fcntl(socket_fd,F_SETFL,flags|O_NONBLOCK)!=0)
        {
            log_line(
                server,
                "CLIENT nonblocking setup failed: errno=%d.",
                errno);
            (void)close(socket_fd);
            continue;
        }

        client=(stn_stratum_client_session *)calloc(
            1u,
            sizeof(*client));

        if(client==NULL){
            log_line(
                server,
                "CLIENT rejected: host memory unavailable.");
            (void)close(socket_fd);
            continue;
        }

        client->socket=(uintptr_t)socket_fd;
        client->next=server->clients;
        server->clients=client;
        server->client_count++;

        log_line(
            server,
            "CLIENT connected; clients=%zu.",
            server->client_count);

        if(server->have_work){
            (void)client_send_job(server,client);
        }
    }
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

    if(client==NULL){return;}

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

    payload=(uint8_t *)malloc(
        server->active_template_length);

    if(payload==NULL){
        log_line(
            server,
            "ERROR submission scratch allocation failed: %zu bytes.",
            server->active_template_length);

        client_send_result(
            server,
            client,
            STN_RPC_CLIENT_PROVIDER);

        return;
    }

    memcpy(
        payload,
        server->active_template,
        server->active_template_length);

    nonce=read64be(p+40);

    {
        uint8_t *nonce_field=
            payload+68u+STN_MINER_CHAIN_NONCE_OFFSET;
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
        server->active_template_length,
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
    size_t frame_size;

    if(client==NULL){return;}

    if(client->socket==STN_INVALID_SOCKET_VALUE){
        return;
    }

    socket_fd=(int)client->socket;

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

        if(errno==EAGAIN || errno==EWOULDBLOCK){
            break;
        }

        client_remove(
            server,
            client,
            "receive failed");

        return;
        }

        if(client->rx_used<frame_size){
            return;
        }

        if(frame_size==STN_MINER_SUBMIT_SIZE){
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
            clear_work(server);
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

    server->listen_socket=
        STN_INVALID_SOCKET_VALUE;

    server->telemetry_socket=
        STN_INVALID_SOCKET_VALUE;

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

    if(!listener_open(server)){
        log_line(
            server,
            "STOP startup failed.");

        return 0;
    }

    if(!telemetry_open(server)){
        log_line(
            server,
            "ERROR unable to open telemetry listener on port %u.",
            (unsigned)STN_STRATUM_TELEMETRY_PORT);
        log_line(server,"STOP startup failed.");
        return 0;
    }

    while(server->running){
        accept_clients(server);
        service_clients(server);
        telemetry_service(server);

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

    telemetry_close(server);
    listener_close(server);

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
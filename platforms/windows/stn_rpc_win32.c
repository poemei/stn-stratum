/* Copyright (c) 2026 STN-Labz. All rights reserved. */
#include "stn_rpc_win32.h"

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>

#include <limits.h>
#include <stdio.h>
#include <string.h>

#pragma comment(lib,"Ws2_32.lib")

#define STN_INVALID_SOCKET_HANDLE ((uintptr_t)INVALID_SOCKET)

static uint32_t read32be(const uint8_t *p)
{
    return ((uint32_t)p[0]<<24)|
           ((uint32_t)p[1]<<16)|
           ((uint32_t)p[2]<<8)|
           (uint32_t)p[3];
}

static SOCKET current_socket(const stn_rpc_win32 *t)
{
    return (SOCKET)t->socket_handle;
}

static void drop_socket(stn_rpc_win32 *t)
{
    SOCKET s;
    if(t==NULL){return;}
    s=current_socket(t);
    if(s!=INVALID_SOCKET){
        closesocket(s);
        t->socket_handle=STN_INVALID_SOCKET_HANDLE;
    }
}

static int ensure_winsock(stn_rpc_win32 *t)
{
    WSADATA wsa;
    if(t->winsock_ready){return 1;}
    if(WSAStartup(MAKEWORD(2,2),&wsa)!=0){return 0;}
    t->winsock_ready=1;
    return 1;
}

static int connect_socket(stn_rpc_win32 *t)
{
    struct addrinfo hints;
    struct addrinfo *result=NULL;
    struct addrinfo *it;
    SOCKET s=INVALID_SOCKET;
    char port_text[16];

    if(current_socket(t)!=INVALID_SOCKET){return 1;}
    if(!ensure_winsock(t)){return 0;}

    memset(&hints,0,sizeof(hints));
    hints.ai_family=AF_UNSPEC;
    hints.ai_socktype=SOCK_STREAM;
    hints.ai_protocol=IPPROTO_TCP;
    (void)snprintf(port_text,sizeof(port_text),"%u",(unsigned)t->port);

    if(getaddrinfo(t->host,port_text,&hints,&result)!=0){
        return 0;
    }

    for(it=result;it!=NULL;it=it->ai_next){
        s=socket(it->ai_family,it->ai_socktype,it->ai_protocol);
        if(s==INVALID_SOCKET){continue;}
        if(connect(s,it->ai_addr,(int)it->ai_addrlen)==0){break;}
        closesocket(s);
        s=INVALID_SOCKET;
    }

    freeaddrinfo(result);

    if(s==INVALID_SOCKET){return 0;}
    {
        DWORD timeout=5000;
        if(setsockopt(s,SOL_SOCKET,SO_RCVTIMEO,(const char *)&timeout,sizeof(timeout))!=0 ||
           setsockopt(s,SOL_SOCKET,SO_SNDTIMEO,(const char *)&timeout,sizeof(timeout))!=0){closesocket(s);return 0;}
    }
    t->socket_handle=(uintptr_t)s;
    return 1;
}

static int send_all(SOCKET s,const uint8_t *p,size_t n)
{
    while(n!=0u){
        int chunk=(n>(size_t)INT_MAX)?INT_MAX:(int)n;
        int sent=send(s,(const char *)p,chunk,0);
        if(sent<=0){return 0;}
        p+=(size_t)sent;
        n-=(size_t)sent;
    }
    return 1;
}

static int recv_all(SOCKET s,uint8_t *p,size_t n)
{
    while(n!=0u){
        int chunk=(n>(size_t)INT_MAX)?INT_MAX:(int)n;
        int got=recv(s,(char *)p,chunk,0);
        if(got<=0){return 0;}
        p+=(size_t)got;
        n-=(size_t)got;
    }
    return 1;
}

static int exchange_once(
    stn_rpc_win32 *t,
    const uint8_t *request,
    size_t request_length,
    uint8_t *response,
    size_t response_capacity,
    size_t *response_length)
{
    SOCKET s;
    uint32_t payload_length;
    size_t frame_length;

    if(!connect_socket(t)){return 0;}
    s=current_socket(t);

    if(!send_all(s,request,request_length)){drop_socket(t);return 0;}
    if(!recv_all(s,response,24u)){drop_socket(t);return 0;}

    payload_length=read32be(response+20);
    frame_length=24u+(size_t)payload_length;
    if(frame_length>response_capacity){drop_socket(t);return 0;}

    if(payload_length!=0u &&
       !recv_all(s,response+24,payload_length))
    {
        drop_socket(t);
        return 0;
    }

    *response_length=frame_length;
    return 1;
}

void stn_rpc_win32_init(
    stn_rpc_win32 *transport,
    const char *host,
    uint16_t port)
{
    if(transport!=NULL){
        transport->host=host;
        transport->port=port;
        transport->socket_handle=STN_INVALID_SOCKET_HANDLE;
        transport->winsock_ready=0;
    }
}

void stn_rpc_win32_close(stn_rpc_win32 *transport)
{
    if(transport==NULL){return;}
    drop_socket(transport);
    if(transport->winsock_ready){
        WSACleanup();
        transport->winsock_ready=0;
    }
}

int stn_rpc_win32_exchange(
    void *user,
    const uint8_t *request,
    size_t request_length,
    uint8_t *response,
    size_t response_capacity,
    size_t *response_length)
{
    stn_rpc_win32 *t=(stn_rpc_win32 *)user;

    if(response_length!=NULL){*response_length=0;}
    if(t==NULL||t->host==NULL||request==NULL||
       response==NULL||response_length==NULL||response_capacity<24u)
    {
        return 0;
    }

    if(exchange_once(
        t,request,request_length,response,response_capacity,response_length))
    {
        return 1;
    }

    /* Only repeat read operations. A lost mutation response has an unknown
     * outcome; retransmitting may misleadingly turn acceptance into STALE. */
    if(request_length<24u || !((request[8]==0 && request[9]==1) ||
       (request[8]==0x20 && request[9]==2))){return 0;}
    return exchange_once(
        t,request,request_length,response,response_capacity,response_length);
}

#else

void stn_rpc_win32_init(
    stn_rpc_win32 *transport,
    const char *host,
    uint16_t port)
{
    if(transport!=NULL){
        transport->host=host;
        transport->port=port;
        transport->socket_handle=0;
        transport->winsock_ready=0;
    }
}

void stn_rpc_win32_close(stn_rpc_win32 *transport)
{
    (void)transport;
}

int stn_rpc_win32_exchange(
    void *user,
    const uint8_t *request,
    size_t request_length,
    uint8_t *response,
    size_t response_capacity,
    size_t *response_length)
{
    (void)user;
    (void)request;
    (void)request_length;
    (void)response;
    (void)response_capacity;
    if(response_length!=NULL){*response_length=0;}
    return 0;
}

#endif

/* Copyright (c) 2026 STN-Labz. All rights reserved. */
#include "stn_rpc_linux.h"

#ifdef __linux__

#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#define STN_INVALID_SOCKET_FD (-1)

static uint32_t read32be(const uint8_t *p)
{
    return ((uint32_t)p[0]<<24)|
           ((uint32_t)p[1]<<16)|
           ((uint32_t)p[2]<<8)|
           (uint32_t)p[3];
}

static void drop_socket(stn_rpc_linux *t)
{
    if(t==NULL){return;}

    if(t->socket_fd!=STN_INVALID_SOCKET_FD){
        (void)close(t->socket_fd);
        t->socket_fd=STN_INVALID_SOCKET_FD;
    }
}

static int connect_socket(stn_rpc_linux *t)
{
    struct addrinfo hints;
    struct addrinfo *result=NULL;
    struct addrinfo *it;
    struct timeval timeout;
    int s=STN_INVALID_SOCKET_FD;
    char port_text[16];

    if(t->socket_fd!=STN_INVALID_SOCKET_FD){return 1;}

    memset(&hints,0,sizeof(hints));
    hints.ai_family=AF_UNSPEC;
    hints.ai_socktype=SOCK_STREAM;
    hints.ai_protocol=IPPROTO_TCP;

    (void)snprintf(
        port_text,
        sizeof(port_text),
        "%u",
        (unsigned)t->port);

    if(getaddrinfo(t->host,port_text,&hints,&result)!=0){
        return 0;
    }

    for(it=result;it!=NULL;it=it->ai_next){
        s=socket(
            it->ai_family,
            it->ai_socktype,
            it->ai_protocol);

        if(s==STN_INVALID_SOCKET_FD){
            continue;
        }

        if(connect(
            s,
            it->ai_addr,
            it->ai_addrlen)==0)
        {
            break;
        }

        (void)close(s);
        s=STN_INVALID_SOCKET_FD;
    }

    freeaddrinfo(result);

    if(s==STN_INVALID_SOCKET_FD){
        return 0;
    }

    timeout.tv_sec=5;
    timeout.tv_usec=0;

    if(setsockopt(
        s,
        SOL_SOCKET,
        SO_RCVTIMEO,
        &timeout,
        sizeof(timeout))!=0 ||
       setsockopt(
        s,
        SOL_SOCKET,
        SO_SNDTIMEO,
        &timeout,
        sizeof(timeout))!=0)
    {
        (void)close(s);
        return 0;
    }

    t->socket_fd=s;
    return 1;
}

static int send_all(
    int s,
    const uint8_t *p,
    size_t n)
{
    while(n!=0u){
        ssize_t sent=send(s,p,n,0);

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

static int recv_all(
    int s,
    uint8_t *p,
    size_t n)
{
    while(n!=0u){
        ssize_t got=recv(s,p,n,0);

        if(got<0){
            if(errno==EINTR){
                continue;
            }

            return 0;
        }

        if(got==0){
            return 0;
        }

        p+=(size_t)got;
        n-=(size_t)got;
    }

    return 1;
}

static int exchange_once(
    stn_rpc_linux *t,
    const uint8_t *request,
    size_t request_length,
    uint8_t *response,
    size_t response_capacity,
    size_t *response_length)
{
    int s;
    uint32_t payload_length;
    size_t frame_length;

    if(!connect_socket(t)){
        return 0;
    }

    s=t->socket_fd;

    if(!send_all(s,request,request_length)){
        drop_socket(t);
        return 0;
    }

    if(!recv_all(s,response,24u)){
        drop_socket(t);
        return 0;
    }

    payload_length=read32be(response+20);
    frame_length=24u+(size_t)payload_length;

    if(frame_length>response_capacity){
        drop_socket(t);
        return 0;
    }

    if(payload_length!=0u &&
       !recv_all(
           s,
           response+24u,
           (size_t)payload_length))
    {
        drop_socket(t);
        return 0;
    }

    *response_length=frame_length;
    return 1;
}

void stn_rpc_linux_init(
    stn_rpc_linux *transport,
    const char *host,
    uint16_t port)
{
    if(transport!=NULL){
        transport->host=host;
        transport->port=port;
        transport->socket_fd=STN_INVALID_SOCKET_FD;
    }
}

void stn_rpc_linux_close(
    stn_rpc_linux *transport)
{
    if(transport==NULL){return;}

    drop_socket(transport);
}

int stn_rpc_linux_exchange(
    void *user,
    const uint8_t *request,
    size_t request_length,
    uint8_t *response,
    size_t response_capacity,
    size_t *response_length)
{
    stn_rpc_linux *t=(stn_rpc_linux *)user;

    if(response_length!=NULL){
        *response_length=0u;
    }

    if(t==NULL ||
       t->host==NULL ||
       request==NULL ||
       response==NULL ||
       response_length==NULL ||
       response_capacity<24u)
    {
        return 0;
    }

    if(exchange_once(
        t,
        request,
        request_length,
        response,
        response_capacity,
        response_length))
    {
        return 1;
    }

    /*
     * Only repeat read operations. A lost mutation response has an
     * unknown outcome; retransmitting may misleadingly turn acceptance
     * into STALE.
     */
    if(request_length<24u ||
       !((request[8]==0x00u && request[9]==0x01u) ||
         (request[8]==0x20u && request[9]==0x02u)))
    {
        return 0;
    }

    return exchange_once(
        t,
        request,
        request_length,
        response,
        response_capacity,
        response_length);
}

#else

void stn_rpc_linux_init(
    stn_rpc_linux *transport,
    const char *host,
    uint16_t port)
{
    if(transport!=NULL){
        transport->host=host;
        transport->port=port;
        transport->socket_fd=-1;
    }
}

void stn_rpc_linux_close(
    stn_rpc_linux *transport)
{
    (void)transport;
}

int stn_rpc_linux_exchange(
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

    if(response_length!=NULL){
        *response_length=0u;
    }

    return 0;
}

#endif
/* Copyright (c) 2026 STN-Labz. All rights reserved. */
#include "stn_mining_listener.h"

#include <string.h>

#ifdef _WIN32
#include <ws2tcpip.h>
#else
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

void stn_mining_listener_init(
    stn_mining_listener *listener,
    stn_service_mode mode)
{
    if(listener==NULL){return;}

    listener->socket=STN_MINING_INVALID_SOCKET;
    listener->mode=mode;
    listener->port=stn_service_mode_port(mode);
}

void stn_mining_socket_close(stn_mining_socket socket_fd)
{
    if(socket_fd==STN_MINING_INVALID_SOCKET){return;}
#ifdef _WIN32
    closesocket(socket_fd);
#else
    (void)close(socket_fd);
#endif
}

int stn_mining_listener_open(stn_mining_listener *listener)
{
    struct sockaddr_in address;
    int reuse=1;

    if(listener==NULL || listener->port==0u){return 0;}

#ifdef _WIN32
    {
        u_long nonblocking=1u;
        SOCKET socket_fd=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);

        if(socket_fd==INVALID_SOCKET){return 0;}
        if(setsockopt(socket_fd,SOL_SOCKET,SO_REUSEADDR,
            (const char *)&reuse,(int)sizeof(reuse))!=0)
        {
            stn_mining_socket_close(socket_fd);
            return 0;
        }

        memset(&address,0,sizeof(address));
        address.sin_family=AF_INET;
        address.sin_addr.s_addr=htonl(INADDR_ANY);
        address.sin_port=htons(listener->port);

        if(bind(socket_fd,(const struct sockaddr *)&address,sizeof(address))!=0 ||
           listen(socket_fd,SOMAXCONN)!=0 ||
           ioctlsocket(socket_fd,FIONBIO,&nonblocking)!=0)
        {
            stn_mining_socket_close(socket_fd);
            return 0;
        }

        listener->socket=socket_fd;
    }
#else
    {
        int flags;
        int socket_fd=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);

        if(socket_fd<0){return 0;}
        if(setsockopt(socket_fd,SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof(reuse))!=0){
            stn_mining_socket_close(socket_fd);
            return 0;
        }

        memset(&address,0,sizeof(address));
        address.sin_family=AF_INET;
        address.sin_addr.s_addr=htonl(INADDR_ANY);
        address.sin_port=htons(listener->port);

        if(bind(socket_fd,(const struct sockaddr *)&address,sizeof(address))!=0 ||
           listen(socket_fd,SOMAXCONN)!=0)
        {
            stn_mining_socket_close(socket_fd);
            return 0;
        }

        flags=fcntl(socket_fd,F_GETFL,0);
        if(flags<0 || fcntl(socket_fd,F_SETFL,flags|O_NONBLOCK)!=0){
            stn_mining_socket_close(socket_fd);
            return 0;
        }

        listener->socket=socket_fd;
    }
#endif

    return 1;
}

void stn_mining_listener_close(stn_mining_listener *listener)
{
    if(listener==NULL || listener->socket==STN_MINING_INVALID_SOCKET){return;}
    stn_mining_socket_close(listener->socket);
    listener->socket=STN_MINING_INVALID_SOCKET;
}

int stn_mining_listener_accept(
    const stn_mining_listener *listener,
    stn_mining_socket *client_socket)
{
    stn_mining_socket accepted;

    if(client_socket!=NULL){
        *client_socket=STN_MINING_INVALID_SOCKET;
    }

    if(listener==NULL || client_socket==NULL ||
       listener->socket==STN_MINING_INVALID_SOCKET)
    {
        return -1;
    }

#ifdef _WIN32
    accepted=accept(listener->socket,NULL,NULL);
    if(accepted==INVALID_SOCKET){
        int error=WSAGetLastError();
        if(error==WSAEWOULDBLOCK){return 0;}
        return -1;
    }

    {
        u_long nonblocking=1u;
        if(ioctlsocket(accepted,FIONBIO,&nonblocking)!=0){
            stn_mining_socket_close(accepted);
            return -1;
        }
    }
#else
    do{
        accepted=accept(listener->socket,NULL,NULL);
    }while(accepted<0 && errno==EINTR);

    if(accepted<0){
        if(errno==EAGAIN || errno==EWOULDBLOCK){return 0;}
        return -1;
    }

    {
        int flags=fcntl(accepted,F_GETFL,0);
        if(flags<0 || fcntl(accepted,F_SETFL,flags|O_NONBLOCK)!=0){
            stn_mining_socket_close(accepted);
            return -1;
        }
    }
#endif

    *client_socket=accepted;
    return 1;
}

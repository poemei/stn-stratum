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
            closesocket(socket_fd);
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
            closesocket(socket_fd);
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
            (void)close(socket_fd);
            return 0;
        }

        memset(&address,0,sizeof(address));
        address.sin_family=AF_INET;
        address.sin_addr.s_addr=htonl(INADDR_ANY);
        address.sin_port=htons(listener->port);

        if(bind(socket_fd,(const struct sockaddr *)&address,sizeof(address))!=0 ||
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

        listener->socket=socket_fd;
    }
#endif

    return 1;
}

void stn_mining_listener_close(stn_mining_listener *listener)
{
    if(listener==NULL || listener->socket==STN_MINING_INVALID_SOCKET){return;}

#ifdef _WIN32
    closesocket(listener->socket);
#else
    (void)close(listener->socket);
#endif

    listener->socket=STN_MINING_INVALID_SOCKET;
}

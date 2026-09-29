/* Copyright (c) 2026 STN-Labz. All rights reserved. */
#include "stn_session_acceptor.h"

#include <stdlib.h>

int stn_session_acceptor_accept(
    const stn_mining_listener *listener,
    stn_client_session **session_out)
{
    stn_mining_socket socket_fd;
    stn_client_session *session;
    stn_client_mode mode;
    int result;

    if(session_out==NULL){return -1;}
    *session_out=NULL;

    if(listener==NULL){return -1;}

    socket_fd=STN_MINING_INVALID_SOCKET;
    result=stn_mining_listener_accept(listener,&socket_fd);
    if(result<=0){return result;}

    stn_client_mode_init(&mode,listener->mode);
    if(!stn_client_mode_valid(&mode)){
        stn_mining_socket_close(socket_fd);
        return -1;
    }

    session=(stn_client_session *)calloc(1u,sizeof(*session));
    if(session==NULL){
        stn_mining_socket_close(socket_fd);
        return -1;
    }

    stn_client_session_init(
        session,
        (stn_client_socket)socket_fd,
        mode);

    *session_out=session;
    return 1;
}

void stn_session_acceptor_release(stn_client_session *session)
{
    if(session==NULL){return;}

    if(session->socket!=STN_CLIENT_INVALID_SOCKET){
        stn_mining_socket_close((stn_mining_socket)session->socket);
        session->socket=STN_CLIENT_INVALID_SOCKET;
    }

    free(session);
}

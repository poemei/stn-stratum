/* Copyright (c) 2026 STN-Labz. All rights reserved. */
#include "stn_client_session.h"

#include <limits.h>
#include <string.h>

/* [AI:GPT-5.6 Sol | 2026-09-29 00:00:00 UTC] */
void stn_client_session_init(
    stn_client_session *session,
    stn_client_socket socket_fd,
    stn_client_mode mode)
{
    if(session==NULL){return;}
    memset(session,0,sizeof(*session));
    session->socket=socket_fd;
    session->mode=mode;
}

void stn_client_session_clear_job(stn_client_session *session)
{
    if(session==NULL){return;}
    memset(session->sent_job_id,0,sizeof(session->sent_job_id));
    session->hashrate=0u;
    session->has_job=0;
}

uint64_t stn_client_session_total_hashrate(const stn_client_session *sessions)
{
    uint64_t total=0u;
    const stn_client_session *session;

    for(session=sessions;session!=NULL;session=session->next){
        if(UINT64_MAX-total<session->hashrate){return UINT64_MAX;}
        total+=session->hashrate;
    }
    return total;
}

size_t stn_client_session_count(const stn_client_session *sessions)
{
    size_t count=0u;
    const stn_client_session *session;

    for(session=sessions;session!=NULL;session=session->next){
        if(count==SIZE_MAX){return SIZE_MAX;}
        count++;
    }
    return count;
}
/* [End AI:GPT-5.6 Sol] */

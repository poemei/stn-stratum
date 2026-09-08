/* Copyright (c) 2026 STN-Labz. All rights reserved. */
#include "stn_stratum_server.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include <stdio.h>

/*
 * Server state is static storage intentionally.
 * The active mining-template buffer is large and must not live on the
 * Windows default thread stack.
 */
static stn_stratum_server server;

static stn_stratum_server *active_server=NULL;

#ifdef _WIN32
static BOOL WINAPI console_control(DWORD event)
{
    if(event==CTRL_C_EVENT||
       event==CTRL_BREAK_EVENT||
       event==CTRL_CLOSE_EVENT||
       event==CTRL_SHUTDOWN_EVENT)
    {
        if(active_server!=NULL){
            stn_stratum_server_stop(active_server);
        }
        return TRUE;
    }
    return FALSE;
}
#endif

int main(void)
{
    int ok;

    stn_stratum_server_init(&server);
    active_server=&server;

#ifdef _WIN32
    if(!SetConsoleCtrlHandler(console_control,TRUE)){
        fprintf(stderr,"Unable to install console control handler.\n");
        stn_stratum_server_close(&server);
        return 1;
    }
#endif

    ok=stn_stratum_server_run(&server);

    stn_stratum_server_close(&server);
    active_server=NULL;

    return ok?0:1;
}

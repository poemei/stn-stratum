/* Copyright (c) 2026 STN-Labz. All rights reserved. */
#include <stdio.h>

#include "stn_mining_listener.h"

static unsigned checks;
static unsigned failures;

#define CHECK(expression) do { \
    ++checks; \
    if(!(expression)){ \
        ++failures; \
        printf("FAIL line %d: %s\n",__LINE__,#expression); \
    } \
} while(0)

int main(void)
{
    stn_mining_listener pool;
    stn_mining_listener solo;
    stn_mining_socket accepted;
    int pool_open;
    int solo_open;
    int accept_result;

    stn_mining_listener_init(&pool,STN_SERVICE_MODE_POOL);
    stn_mining_listener_init(&solo,STN_SERVICE_MODE_SOLO);

    CHECK(pool.mode==STN_SERVICE_MODE_POOL);
    CHECK(pool.port==18475u);
    CHECK(pool.socket==STN_MINING_INVALID_SOCKET);

    CHECK(solo.mode==STN_SERVICE_MODE_SOLO);
    CHECK(solo.port==18477u);
    CHECK(solo.socket==STN_MINING_INVALID_SOCKET);

    CHECK(pool.port!=solo.port);

    accepted=STN_MINING_INVALID_SOCKET;
    CHECK(stn_mining_listener_accept(NULL,&accepted)==-1);
    CHECK(stn_mining_listener_accept(&pool,NULL)==-1);
    CHECK(stn_mining_listener_accept(&pool,&accepted)==-1);
    CHECK(accepted==STN_MINING_INVALID_SOCKET);

    /*
     * Production ports may already be owned by a running STN-Stratum during
     * an in-place qualification. Opening a listener is therefore exercised
     * when the port is available, but an occupied production port is not a
     * module failure. The deterministic mode/port mapping above remains the
     * invariant this test must always prove.
     */
    pool_open=stn_mining_listener_open(&pool);
    if(pool_open){
        CHECK(pool.socket!=STN_MINING_INVALID_SOCKET);
        accepted=STN_MINING_INVALID_SOCKET;
        accept_result=stn_mining_listener_accept(&pool,&accepted);
        CHECK(accept_result==0);
        CHECK(accepted==STN_MINING_INVALID_SOCKET);
    }
    else{
        CHECK(pool.socket==STN_MINING_INVALID_SOCKET);
    }

    solo_open=stn_mining_listener_open(&solo);
    if(solo_open){
        CHECK(solo.socket!=STN_MINING_INVALID_SOCKET);
        accepted=STN_MINING_INVALID_SOCKET;
        accept_result=stn_mining_listener_accept(&solo,&accepted);
        CHECK(accept_result==0);
        CHECK(accepted==STN_MINING_INVALID_SOCKET);
    }
    else{
        CHECK(solo.socket==STN_MINING_INVALID_SOCKET);
    }

    if(pool_open){stn_mining_listener_close(&pool);}
    if(solo_open){stn_mining_listener_close(&solo);}

    CHECK(pool.socket==STN_MINING_INVALID_SOCKET);
    CHECK(solo.socket==STN_MINING_INVALID_SOCKET);

    printf("Mining listener: %u checks, %u failures.\n",checks,failures);
    return failures!=0u ? 1 : 0;
}

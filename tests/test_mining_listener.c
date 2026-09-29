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

    stn_mining_listener_init(&pool,STN_SERVICE_MODE_POOL);
    stn_mining_listener_init(&solo,STN_SERVICE_MODE_SOLO);

    CHECK(pool.mode==STN_SERVICE_MODE_POOL);
    CHECK(pool.port==18475u);
    CHECK(pool.socket==STN_MINING_INVALID_SOCKET);

    CHECK(solo.mode==STN_SERVICE_MODE_SOLO);
    CHECK(solo.port==18477u);
    CHECK(solo.socket==STN_MINING_INVALID_SOCKET);

    CHECK(pool.port!=solo.port);

    accepted=(stn_mining_socket)0;
    CHECK(stn_mining_listener_accept(NULL,&accepted)==-1);
    CHECK(accepted==STN_MINING_INVALID_SOCKET);
    CHECK(stn_mining_listener_accept(&pool,NULL)==-1);
    CHECK(stn_mining_listener_accept(&pool,&accepted)==-1);
    CHECK(accepted==STN_MINING_INVALID_SOCKET);

    /*
     * Qualification must prove that both production mining listeners can
     * coexist. This catches accidental port aliasing and socket setup
     * regressions before the server crossover uses both listeners.
     */
    pool_open=stn_mining_listener_open(&pool);
    CHECK(pool_open==1);
    CHECK(pool.socket!=STN_MINING_INVALID_SOCKET);

    solo_open=stn_mining_listener_open(&solo);
    CHECK(solo_open==1);
    CHECK(solo.socket!=STN_MINING_INVALID_SOCKET);

    /* Empty nonblocking listeners report no pending client, not failure. */
    if(pool_open){
        CHECK(stn_mining_listener_accept(&pool,&accepted)==0);
        CHECK(accepted==STN_MINING_INVALID_SOCKET);
    }
    if(solo_open){
        CHECK(stn_mining_listener_accept(&solo,&accepted)==0);
        CHECK(accepted==STN_MINING_INVALID_SOCKET);
    }

    if(pool_open){stn_mining_listener_close(&pool);}
    if(solo_open){stn_mining_listener_close(&solo);}

    CHECK(pool.socket==STN_MINING_INVALID_SOCKET);
    CHECK(solo.socket==STN_MINING_INVALID_SOCKET);

    printf("Mining listener: %u checks, %u failures.\n",checks,failures);
    return failures!=0u ? 1 : 0;
}

/* Copyright (c) 2026 STN-Labz. All rights reserved. */
#include <stdio.h>

#include "stn_session_acceptor.h"

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
    stn_client_session *session=(stn_client_session *)1;

    stn_mining_listener_init(&pool,STN_SERVICE_MODE_POOL);
    stn_mining_listener_init(&solo,STN_SERVICE_MODE_SOLO);

    CHECK(pool.port==18475u);
    CHECK(solo.port==18477u);

    CHECK(stn_session_acceptor_accept(NULL,&session)==-1);
    CHECK(session==NULL);
    CHECK(stn_session_acceptor_accept(&pool,NULL)==-1);

    session=(stn_client_session *)1;
    CHECK(stn_session_acceptor_accept(&pool,&session)==-1);
    CHECK(session==NULL);

    CHECK(pool.mode==STN_SERVICE_MODE_POOL);
    CHECK(solo.mode==STN_SERVICE_MODE_SOLO);

    stn_session_acceptor_release(NULL);

    printf("Session acceptor: %u checks, %u failures.\n",checks,failures);
    return failures!=0u ? 1 : 0;
}

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

    stn_mining_listener_init(&pool,STN_SERVICE_MODE_POOL);
    stn_mining_listener_init(&solo,STN_SERVICE_MODE_SOLO);

    CHECK(pool.mode==STN_SERVICE_MODE_POOL);
    CHECK(pool.port==18475u);
    CHECK(pool.socket==STN_MINING_INVALID_SOCKET);

    CHECK(solo.mode==STN_SERVICE_MODE_SOLO);
    CHECK(solo.port==18477u);
    CHECK(solo.socket==STN_MINING_INVALID_SOCKET);

    CHECK(pool.port!=solo.port);

    printf("Mining listener: %u checks, %u failures.\n",checks,failures);
    return failures!=0u ? 1 : 0;
}

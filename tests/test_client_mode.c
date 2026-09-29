/* Copyright (c) 2026 STN-Labz. All rights reserved. */
#include <stdio.h>

#include "stn_client_mode.h"

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
    stn_client_mode pool;
    stn_client_mode solo;
    stn_client_mode invalid;

    stn_client_mode_init(&pool,STN_SERVICE_MODE_POOL);
    stn_client_mode_init(&solo,STN_SERVICE_MODE_SOLO);

    invalid.service_mode=(stn_service_mode)0;
    invalid.service_port=18475u;

    CHECK(stn_client_mode_valid(&pool)==1);
    CHECK(pool.service_port==18475u);
    CHECK(stn_client_mode_is_pool(&pool)==1);
    CHECK(stn_client_mode_is_solo(&pool)==0);

    CHECK(stn_client_mode_valid(&solo)==1);
    CHECK(solo.service_port==18477u);
    CHECK(stn_client_mode_is_pool(&solo)==0);
    CHECK(stn_client_mode_is_solo(&solo)==1);

    CHECK(stn_client_mode_valid(&invalid)==0);
    CHECK(stn_client_mode_valid(NULL)==0);

    printf("Client mode: %u checks, %u failures.\n",checks,failures);
    return failures!=0u ? 1 : 0;
}

/* Copyright (c) 2026 STN-Labz. All rights reserved. */
#include <stdio.h>
#include <string.h>

#include "stn_service_mode.h"
#include "stn_stratum_server.h"

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
    CHECK(stn_service_mode_port(STN_SERVICE_MODE_POOL)==STN_STRATUM_POOL_PORT);
    CHECK(stn_service_mode_port(STN_SERVICE_MODE_SOLO)==STN_STRATUM_SOLO_PORT);
    CHECK(stn_service_mode_port((stn_service_mode)0)==0u);

    CHECK(strcmp(stn_service_mode_name(STN_SERVICE_MODE_POOL),"pool")==0);
    CHECK(strcmp(stn_service_mode_name(STN_SERVICE_MODE_SOLO),"solo")==0);
    CHECK(strcmp(stn_service_mode_name((stn_service_mode)0),"invalid")==0);

    CHECK(STN_STRATUM_POOL_PORT!=STN_STRATUM_SOLO_PORT);
    CHECK(STN_STRATUM_POOL_PORT!=STN_STRATUM_TELEMETRY_PORT);
    CHECK(STN_STRATUM_SOLO_PORT!=STN_STRATUM_TELEMETRY_PORT);

    printf("Service mode: %u checks, %u failures.\n",checks,failures);
    return failures!=0u ? 1 : 0;
}

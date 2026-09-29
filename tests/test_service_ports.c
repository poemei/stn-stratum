/* Copyright (c) 2026 STN-Labz. All rights reserved. */
#include <stdio.h>

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
    CHECK(STN_STRATUM_POOL_PORT==18475u);
    CHECK(STN_STRATUM_DEFAULT_PORT==STN_STRATUM_POOL_PORT);
    CHECK(STN_STRATUM_TELEMETRY_PORT==18476u);
    CHECK(STN_STRATUM_SOLO_PORT==18477u);

    CHECK(STN_STRATUM_POOL_PORT!=STN_STRATUM_TELEMETRY_PORT);
    CHECK(STN_STRATUM_POOL_PORT!=STN_STRATUM_SOLO_PORT);
    CHECK(STN_STRATUM_TELEMETRY_PORT!=STN_STRATUM_SOLO_PORT);

    printf("Stratum service ports: %u checks, %u failures.\n",checks,failures);
    return failures!=0u ? 1 : 0;
}

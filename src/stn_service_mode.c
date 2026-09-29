/* Copyright (c) 2026 STN-Labz. All rights reserved. */
#include "stn_service_mode.h"
#include "stn_stratum_server.h"

uint16_t stn_service_mode_port(stn_service_mode mode)
{
    switch(mode){
    case STN_SERVICE_MODE_POOL:
        return (uint16_t)STN_STRATUM_POOL_PORT;
    case STN_SERVICE_MODE_SOLO:
        return (uint16_t)STN_STRATUM_SOLO_PORT;
    default:
        return 0u;
    }
}

const char *stn_service_mode_name(stn_service_mode mode)
{
    switch(mode){
    case STN_SERVICE_MODE_POOL:
        return "pool";
    case STN_SERVICE_MODE_SOLO:
        return "solo";
    default:
        return "invalid";
    }
}

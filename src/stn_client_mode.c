/* Copyright (c) 2026 STN-Labz. All rights reserved. */
#include "stn_client_mode.h"

void stn_client_mode_init(
    stn_client_mode *client_mode,
    stn_service_mode service_mode)
{
    if(client_mode==NULL){return;}

    client_mode->service_mode=service_mode;
    client_mode->service_port=stn_service_mode_port(service_mode);
}

int stn_client_mode_valid(const stn_client_mode *client_mode)
{
    if(client_mode==NULL){return 0;}

    return client_mode->service_port!=0u &&
        client_mode->service_port==
            stn_service_mode_port(client_mode->service_mode);
}

int stn_client_mode_is_pool(const stn_client_mode *client_mode)
{
    return stn_client_mode_valid(client_mode) &&
        client_mode->service_mode==STN_SERVICE_MODE_POOL;
}

int stn_client_mode_is_solo(const stn_client_mode *client_mode)
{
    return stn_client_mode_valid(client_mode) &&
        client_mode->service_mode==STN_SERVICE_MODE_SOLO;
}

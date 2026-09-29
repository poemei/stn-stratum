/* Copyright (c) 2026 STN-Labz. All rights reserved. */
#ifndef STN_SERVICE_MODE_H
#define STN_SERVICE_MODE_H

#include <stdint.h>

typedef enum stn_service_mode {
    STN_SERVICE_MODE_POOL=1,
    STN_SERVICE_MODE_SOLO=2
} stn_service_mode;

uint16_t stn_service_mode_port(stn_service_mode mode);
const char *stn_service_mode_name(stn_service_mode mode);

#endif

/* Copyright (c) 2026 STN-Labz. All rights reserved. */
#include <stdio.h>
#include <string.h>

#include "stn_telemetry.h"

static unsigned checks;
static unsigned failures;

#define CHECK(expression) do { \
    ++checks; \
    if(!(expression)){ \
        ++failures; \
        printf("FAIL line %d: %s\n",__LINE__,#expression); \
    } \
} while(0)

/* [AI:GPT-5.6 Sol | 2026-09-29 00:00:00 UTC] */
int main(void)
{
    static const char status_request[]="GET /status HTTP/1.1\r\nHost: localhost\r\n\r\n";
    static const char root_request[]="GET / HTTP/1.1\r\nHost: localhost\r\n\r\n";
    static const char bad_request[]="POST /status HTTP/1.1\r\n\r\n";
    stn_telemetry_status status;
    stn_telemetry_service service;
    char json[2048];
    char response[4096];
    int json_length;
    int response_length;

    CHECK(stn_telemetry_request_valid(status_request,sizeof(status_request)-1u)==1);
    CHECK(stn_telemetry_request_valid(root_request,sizeof(root_request)-1u)==1);
    CHECK(stn_telemetry_request_valid(bad_request,sizeof(bad_request)-1u)==0);
    CHECK(stn_telemetry_request_valid(NULL,0u)==0);

    memset(&status,0,sizeof(status));
    status.chain_connected=1;
    status.chain_host="127.0.0.1";
    status.chain_port=18473u;
    status.work_available=1;
    status.miners=4u;
    status.hashrate=123456u;
    status.job_id="0123456789abcdef";
    status.base_id="fedcba9876543210";
    status.target="00000000ffffffff";
    status.uptime_seconds=3600u;

    json_length=stn_telemetry_status_json(&status,json,sizeof(json));
    CHECK(json_length>0);
    CHECK(strstr(json,"\"service\":\"STN-Stratum\"")!=NULL);
    CHECK(strstr(json,"\"chain_connected\":true")!=NULL);
    CHECK(strstr(json,"\"chain_port\":18473")!=NULL);
    CHECK(strstr(json,"\"miners\":4")!=NULL);
    CHECK(strstr(json,"\"hashrate\":123456")!=NULL);
    CHECK(strstr(json,"\"uptime_seconds\":3600")!=NULL);

    response_length=stn_telemetry_http_response(&status,response,sizeof(response));
    CHECK(response_length>0);
    CHECK(strncmp(response,"HTTP/1.1 200 OK\r\n",17u)==0);
    CHECK(strstr(response,"Content-Type: application/json\r\n")!=NULL);
    CHECK(strstr(response,"Cache-Control: no-store\r\n")!=NULL);
    CHECK(strstr(response,"\r\n\r\n{\"service\":\"STN-Stratum\"")!=NULL);

    stn_telemetry_service_init(&service,18476u);
    CHECK(service.listen_socket==STN_TELEMETRY_INVALID_SOCKET);
    CHECK(service.clients==NULL);
    CHECK(service.client_count==0u);
    CHECK(service.port==18476u);

    stn_telemetry_service_close(&service);
    CHECK(service.listen_socket==STN_TELEMETRY_INVALID_SOCKET);
    CHECK(service.clients==NULL);
    CHECK(service.client_count==0u);

    printf("Telemetry module: %u checks, %u failures.\n",checks,failures);
    return failures!=0u ? 1 : 0;
}
/* [End AI:GPT-5.6 Sol] */

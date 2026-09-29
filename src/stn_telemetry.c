/* Copyright (c) 2026 STN-Labz. All rights reserved. */
#include "stn_telemetry.h"

#include <stdio.h>
#include <string.h>

/* [AI:GPT-5.6 Sol | 2026-09-29 00:00:00 UTC] */
int stn_telemetry_request_valid(const char *request, size_t request_length)
{
    static const char status_request[]="GET /status ";
    static const char root_request[]="GET / ";

    if(request==NULL || request_length==0u){return 0;}

    if(request_length>=sizeof(status_request)-1u &&
       memcmp(request,status_request,sizeof(status_request)-1u)==0)
    {
        return 1;
    }

    if(request_length>=sizeof(root_request)-1u &&
       memcmp(request,root_request,sizeof(root_request)-1u)==0)
    {
        return 1;
    }

    return 0;
}

int stn_telemetry_status_json(
    const stn_telemetry_status *status,
    char *output,
    size_t output_size)
{
    int length;

    if(status==NULL || output==NULL || output_size==0u ||
       status->chain_host==NULL || status->job_id==NULL ||
       status->base_id==NULL || status->target==NULL)
    {
        return 0;
    }

    length=snprintf(
        output,
        output_size,
        "{\"service\":\"STN-Stratum\",\"status\":\"running\","
        "\"chain_connected\":%s,\"chain_host\":\"%s\","
        "\"chain_port\":%u,\"work_available\":%s,\"miners\":%zu,"
        "\"hashrate\":%llu,\"job_id\":\"%s\",\"base_id\":\"%s\","
        "\"target\":\"%s\",\"uptime_seconds\":%llu}\n",
        status->chain_connected?"true":"false",
        status->chain_host,
        (unsigned)status->chain_port,
        status->work_available?"true":"false",
        status->miners,
        (unsigned long long)status->hashrate,
        status->job_id,
        status->base_id,
        status->target,
        (unsigned long long)status->uptime_seconds);

    if(length<0 || (size_t)length>=output_size){return 0;}
    return length;
}

int stn_telemetry_http_response(
    const stn_telemetry_status *status,
    char *output,
    size_t output_size)
{
    char body[2048];
    int body_length;
    int response_length;

    if(output==NULL || output_size==0u){return 0;}

    body_length=stn_telemetry_status_json(status,body,sizeof(body));
    if(body_length<=0){return 0;}

    response_length=snprintf(
        output,
        output_size,
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: %d\r\n"
        "Cache-Control: no-store\r\n"
        "Connection: close\r\n"
        "\r\n%s",
        body_length,
        body);

    if(response_length<0 || (size_t)response_length>=output_size){return 0;}
    return response_length;
}
/* [End AI:GPT-5.6 Sol] */

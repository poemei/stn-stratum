/* Copyright (c) 2026 STN-Labz. All rights reserved. */
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "stn_telemetry.h"

#ifndef _WIN32
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

static unsigned checks;
static unsigned failures;

#define CHECK(expression) do { \
    ++checks; \
    if(!(expression)){ \
        ++failures; \
        printf("FAIL line %d: %s\n",__LINE__,#expression); \
    } \
} while(0)

#ifndef _WIN32
static void delay_ms(unsigned milliseconds)
{
    struct timespec request;
    struct timespec remaining;

    request.tv_sec=(time_t)(milliseconds/1000u);
    request.tv_nsec=(long)(milliseconds%1000u)*1000000L;
    while(nanosleep(&request,&remaining)!=0){
        if(errno!=EINTR){return;}
        request=remaining;
    }
}

static int request_status(uint16_t port,char *buffer,size_t buffer_size)
{
    static const char request[]=
        "GET /status HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Connection: close\r\n\r\n";
    int socket_fd;
    int flags;
    struct sockaddr_in address;
    size_t used=0u;
    unsigned attempt;

    socket_fd=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
    if(socket_fd<0){return 0;}

    memset(&address,0,sizeof(address));
    address.sin_family=AF_INET;
    address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
    address.sin_port=htons(port);

    if(connect(socket_fd,(const struct sockaddr *)&address,sizeof(address))!=0){
        (void)close(socket_fd);
        return 0;
    }

    if(send(socket_fd,request,sizeof(request)-1u,0)!=(ssize_t)(sizeof(request)-1u)){
        (void)close(socket_fd);
        return 0;
    }

    flags=fcntl(socket_fd,F_GETFL,0);
    if(flags<0 || fcntl(socket_fd,F_SETFL,flags|O_NONBLOCK)!=0){
        (void)close(socket_fd);
        return 0;
    }

    for(attempt=0u;attempt<200u && used+1u<buffer_size;attempt++){
        ssize_t got=recv(socket_fd,buffer+used,buffer_size-1u-used,0);
        if(got>0){
            used+=(size_t)got;
            buffer[used]='\0';
            if(strstr(buffer,"\"service\":\"STN-Stratum\"")!=NULL){
                (void)close(socket_fd);
                return 1;
            }
        }else if(got==0){
            break;
        }else if(errno!=EAGAIN && errno!=EWOULDBLOCK && errno!=EINTR){
            break;
        }
        delay_ms(5u);
    }

    (void)close(socket_fd);
    return 0;
}
#endif

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
    CHECK(service.worker_started==0);

    stn_telemetry_service_close(&service);
    CHECK(service.listen_socket==STN_TELEMETRY_INVALID_SOCKET);
    CHECK(service.clients==NULL);
    CHECK(service.client_count==0u);

#ifndef _WIN32
    {
        stn_telemetry_service live;
        const uint16_t test_port=19476u;
        char received[4096];

        stn_telemetry_service_init(&live,test_port);
        CHECK(stn_telemetry_service_open(&live)==1);
        CHECK(live.listen_socket!=STN_TELEMETRY_INVALID_SOCKET);
        CHECK(live.worker_started==1);

        stn_telemetry_service_tick(&live,&status,time(NULL));
        memset(received,0,sizeof(received));
        CHECK(request_status(test_port,received,sizeof(received))==1);
        CHECK(strncmp(received,"HTTP/1.1 200 OK\r\n",17u)==0);
        CHECK(strstr(received,"\"miners\":4")!=NULL);

        /*
         * Regression: the caller deliberately stops publishing for one full
         * second. The telemetry worker must continue accepting and answering
         * requests without any help from the Stratum mining/Chain loop.
         */
        delay_ms(1000u);
        memset(received,0,sizeof(received));
        CHECK(request_status(test_port,received,sizeof(received))==1);
        CHECK(strstr(received,"\"hashrate\":123456")!=NULL);

        status.miners=7u;
        status.hashrate=777777u;
        stn_telemetry_service_tick(&live,&status,time(NULL));
        delay_ms(50u);
        memset(received,0,sizeof(received));
        CHECK(request_status(test_port,received,sizeof(received))==1);
        CHECK(strstr(received,"\"miners\":7")!=NULL);
        CHECK(strstr(received,"\"hashrate\":777777")!=NULL);

        stn_telemetry_service_close(&live);
        CHECK(live.worker_started==0);
        CHECK(live.listen_socket==STN_TELEMETRY_INVALID_SOCKET);
        CHECK(live.clients==NULL);
        CHECK(live.client_count==0u);
    }
#endif

    printf("Telemetry module: %u checks, %u failures.\n",checks,failures);
    return failures!=0u ? 1 : 0;
}

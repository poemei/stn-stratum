/* Copyright (c) 2026 STN-Labz. All rights reserved. */
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

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
    int pair[2];
    char request[64];
    static const char status_request[]="GET /status HTTP/1.1\r\n\r\n";
    ssize_t got;

    CHECK(socketpair(AF_UNIX,SOCK_STREAM,0,pair)==0);
    if(failures!=0u){
        printf("Telemetry receive: %u checks, %u failures.\n",checks,failures);
        return 1;
    }

    errno=0;
    got=recv(pair[0],request,sizeof(request),0);
    CHECK(got<0);
    CHECK(errno==EAGAIN || errno==EWOULDBLOCK);

    CHECK(write(pair[1],status_request,sizeof(status_request)-1u)==
        (ssize_t)(sizeof(status_request)-1u));

    memset(request,0,sizeof(request));
    got=recv(pair[0],request,sizeof(request)-1u,0);
    CHECK(got==(ssize_t)(sizeof(status_request)-1u));
    CHECK(memcmp(request,status_request,sizeof(status_request)-1u)==0);

    (void)close(pair[0]);
    (void)close(pair[1]);

    printf("Telemetry receive: %u checks, %u failures.\n",checks,failures);
    return failures!=0u ? 1 : 0;
}

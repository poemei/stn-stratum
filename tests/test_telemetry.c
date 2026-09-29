/* Copyright (c) 2026 STN-Labz. All rights reserved. */
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
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

static int headers_complete(const char *request,size_t length)
{
    size_t i;

    if(request==NULL || length<4u){return 0;}

    for(i=3u;i<length;i++){
        if(request[i-3u]=='\r' &&
           request[i-2u]=='\n' &&
           request[i-1u]=='\r' &&
           request[i]=='\n')
        {
            return 1;
        }
    }

    return 0;
}

int main(void)
{
    int pair[2];
    char request[512];
    static const char part1[]=
        "GET /status HTTP/1.1\r\n"
        "Host: 127.0.0.1:18476\r\n";
    static const char part2[]=
        "User-Agent: PHP\r\n"
        "Accept: */*\r\n";
    static const char part3[]="\r\n";
    struct timeval timeout;
    size_t used=0u;
    ssize_t got;

    CHECK(socketpair(AF_UNIX,SOCK_STREAM,0,pair)==0);
    if(failures!=0u){
        printf("Telemetry receive: %u checks, %u failures.\n",checks,failures);
        return 1;
    }

    timeout.tv_sec=3;
    timeout.tv_usec=0;
    CHECK(setsockopt(pair[0],SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout))==0);

    errno=0;
    got=recv(pair[0],request,sizeof(request),MSG_DONTWAIT);
    CHECK(got<0);
    CHECK(errno==EAGAIN || errno==EWOULDBLOCK);

    CHECK(write(pair[1],part1,sizeof(part1)-1u)==(ssize_t)(sizeof(part1)-1u));
    got=recv(pair[0],request+used,sizeof(request)-1u-used,0);
    CHECK(got==(ssize_t)(sizeof(part1)-1u));
    if(got>0){used+=(size_t)got;}
    CHECK(!headers_complete(request,used));

    CHECK(write(pair[1],part2,sizeof(part2)-1u)==(ssize_t)(sizeof(part2)-1u));
    got=recv(pair[0],request+used,sizeof(request)-1u-used,0);
    CHECK(got==(ssize_t)(sizeof(part2)-1u));
    if(got>0){used+=(size_t)got;}
    CHECK(!headers_complete(request,used));

    CHECK(write(pair[1],part3,sizeof(part3)-1u)==(ssize_t)(sizeof(part3)-1u));
    got=recv(pair[0],request+used,sizeof(request)-1u-used,0);
    CHECK(got==(ssize_t)(sizeof(part3)-1u));
    if(got>0){used+=(size_t)got;}
    request[used]='\0';

    CHECK(headers_complete(request,used));
    CHECK(strncmp(request,"GET /status ",12)==0);
    CHECK(strstr(request,"Host: 127.0.0.1:18476\r\n")!=NULL);
    CHECK(strstr(request,"User-Agent: PHP\r\n")!=NULL);
    CHECK(strstr(request,"Accept: */*\r\n")!=NULL);

    (void)close(pair[0]);
    (void)close(pair[1]);

    printf("Telemetry receive: %u checks, %u failures.\n",checks,failures);
    return failures!=0u ? 1 : 0;
}

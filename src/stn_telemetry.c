/* Copyright (c) 2026 STN-Labz. All rights reserved. */
#include "stn_telemetry.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <ws2tcpip.h>
#else
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

static int request_complete(const char *request,size_t request_length)
{
    size_t i;

    if(request==NULL || request_length<4u){return 0;}
    for(i=3u;i<request_length;i++){
        if(request[i-3u]=='\r' && request[i-2u]=='\n' &&
           request[i-1u]=='\r' && request[i]=='\n')
        {
            return 1;
        }
    }
    return 0;
}

static void copy_text(char *output,size_t output_size,const char *input)
{
    size_t length;

    if(output==NULL || output_size==0u){return;}
    output[0]='\0';
    if(input==NULL){return;}

    length=strlen(input);
    if(length>=output_size){length=output_size-1u;}
    if(length!=0u){memcpy(output,input,length);}
    output[length]='\0';
}

static void socket_close(stn_telemetry_socket socket_fd)
{
#ifdef _WIN32
    closesocket(socket_fd);
#else
    (void)close(socket_fd);
#endif
}

static int socket_would_block(void)
{
#ifdef _WIN32
    int error=WSAGetLastError();
    return error==WSAEWOULDBLOCK;
#else
    return errno==EAGAIN || errno==EWOULDBLOCK;
#endif
}

static int socket_interrupted(void)
{
#ifdef _WIN32
    return WSAGetLastError()==WSAEINTR;
#else
    return errno==EINTR;
#endif
}

static int socket_nonblocking(stn_telemetry_socket socket_fd)
{
#ifdef _WIN32
    u_long enabled=1u;
    return ioctlsocket(socket_fd,FIONBIO,&enabled)==0;
#else
    int flags=fcntl(socket_fd,F_GETFL,0);
    return flags>=0 && fcntl(socket_fd,F_SETFL,flags|O_NONBLOCK)==0;
#endif
}

static void client_remove(
    stn_telemetry_service *service,
    stn_telemetry_client *client)
{
    stn_telemetry_client **link;

    if(service==NULL || client==NULL){return;}
    link=&service->clients;
    while(*link!=NULL && *link!=client){link=&(*link)->next;}
    if(*link==NULL){return;}

    *link=client->next;
    if(client->socket!=STN_TELEMETRY_INVALID_SOCKET){
        socket_close(client->socket);
    }
    if(service->client_count>0u){service->client_count--;}
    free(client);
}

int stn_telemetry_request_valid(const char *request,size_t request_length)
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

static void snapshot_zero(stn_telemetry_snapshot *snapshot)
{
    if(snapshot==NULL){return;}
    memset(snapshot,0,sizeof(*snapshot));
    memset(snapshot->job_id,'0',64u);
    memset(snapshot->base_id,'0',64u);
    memset(snapshot->target,'0',64u);
    snapshot->job_id[64]='\0';
    snapshot->base_id[64]='\0';
    snapshot->target[64]='\0';
}

void stn_telemetry_service_init(
    stn_telemetry_service *service,
    uint16_t port)
{
    if(service==NULL){return;}
    memset(service,0,sizeof(*service));
    service->listen_socket=STN_TELEMETRY_INVALID_SOCKET;
    service->port=port;
    atomic_init(&service->worker_running,0);
    atomic_init(&service->snapshot_sequence,0u);
    snapshot_zero(&service->snapshot);
}

static void snapshot_publish(
    stn_telemetry_service *service,
    const stn_telemetry_status *status)
{
    unsigned sequence;

    sequence=atomic_load_explicit(
        &service->snapshot_sequence,memory_order_relaxed);
    atomic_store_explicit(
        &service->snapshot_sequence,sequence+1u,memory_order_release);

    service->snapshot.chain_connected=status->chain_connected;
    copy_text(service->snapshot.chain_host,
        sizeof(service->snapshot.chain_host),status->chain_host);
    service->snapshot.chain_port=status->chain_port;
    service->snapshot.work_available=status->work_available;
    service->snapshot.miners=status->miners;
    service->snapshot.hashrate=status->hashrate;
    copy_text(service->snapshot.job_id,
        sizeof(service->snapshot.job_id),status->job_id);
    copy_text(service->snapshot.base_id,
        sizeof(service->snapshot.base_id),status->base_id);
    copy_text(service->snapshot.target,
        sizeof(service->snapshot.target),status->target);
    service->snapshot.uptime_seconds=status->uptime_seconds;

    atomic_store_explicit(
        &service->snapshot_sequence,sequence+2u,memory_order_release);
}

static void snapshot_read(
    stn_telemetry_service *service,
    stn_telemetry_snapshot *snapshot)
{
    unsigned before;
    unsigned after;

    do{
        before=atomic_load_explicit(
            &service->snapshot_sequence,memory_order_acquire);
        if((before&1u)!=0u){continue;}
        *snapshot=service->snapshot;
        after=atomic_load_explicit(
            &service->snapshot_sequence,memory_order_acquire);
    }while(before!=after || (after&1u)!=0u);
}

static void accept_clients(stn_telemetry_service *service,time_t now)
{
    for(;;){
        stn_telemetry_socket socket_fd;
        stn_telemetry_client *client;

        socket_fd=accept(service->listen_socket,NULL,NULL);
        if(socket_fd==STN_TELEMETRY_INVALID_SOCKET){
            if(socket_would_block() || socket_interrupted()){return;}
            return;
        }

        if(!socket_nonblocking(socket_fd)){
            socket_close(socket_fd);
            continue;
        }

        client=(stn_telemetry_client *)calloc(1u,sizeof(*client));
        if(client==NULL){
            socket_close(socket_fd);
            continue;
        }

        client->socket=socket_fd;
        client->connected_at=now;
        client->next=service->clients;
        service->clients=client;
        service->client_count++;
    }
}

static int prepare_response(
    stn_telemetry_client *client,
    const stn_telemetry_status *status)
{
    static const char not_found[]=
        "HTTP/1.1 404 Not Found\r\n"
        "Content-Length: 0\r\n"
        "Connection: close\r\n\r\n";
    int length;

    if(stn_telemetry_request_valid(client->request,client->request_length)){
        length=stn_telemetry_http_response(
            status,client->response,sizeof(client->response));
        if(length<=0){return 0;}
        client->response_length=(size_t)length;
    }else{
        memcpy(client->response,not_found,sizeof(not_found)-1u);
        client->response_length=sizeof(not_found)-1u;
    }

    client->response_sent=0u;
    return 1;
}

static int client_tick(
    stn_telemetry_client *client,
    const stn_telemetry_status *status,
    time_t now)
{
    if(client->response_length==0u){
        for(;;){
            int got;
            size_t remaining=sizeof(client->request)-1u-client->request_length;

            if(remaining==0u){return 0;}
#ifdef _WIN32
            got=recv(client->socket,client->request+(int)client->request_length,
                (int)remaining,0);
#else
            got=(int)recv(client->socket,client->request+client->request_length,
                remaining,0);
#endif
            if(got>0){
                client->request_length+=(size_t)got;
                client->request[client->request_length]='\0';
                if(request_complete(client->request,client->request_length)){
                    if(!prepare_response(client,status)){return 0;}
                    break;
                }
                continue;
            }
            if(got==0){return 0;}
            if(socket_interrupted()){continue;}
            if(socket_would_block()){break;}
            return 0;
        }
    }

    if(client->response_length>0u){
        while(client->response_sent<client->response_length){
            int sent;
            size_t remaining=client->response_length-client->response_sent;
#ifdef _WIN32
            sent=send(client->socket,
                client->response+(int)client->response_sent,(int)remaining,0);
#else
            sent=(int)send(client->socket,
                client->response+client->response_sent,remaining,0);
#endif
            if(sent>0){
                client->response_sent+=(size_t)sent;
                continue;
            }
            if(sent==0){return 0;}
            if(socket_interrupted()){continue;}
            if(socket_would_block()){break;}
            return 0;
        }
        if(client->response_sent==client->response_length){return 0;}
    }

    if(client->connected_at!=(time_t)0 && now>=client->connected_at &&
       (uint64_t)(now-client->connected_at)>=STN_TELEMETRY_CLIENT_TIMEOUT_SECONDS)
    {
        return 0;
    }

    return 1;
}

static void network_tick(stn_telemetry_service *service)
{
    stn_telemetry_snapshot snapshot;
    stn_telemetry_status status;
    stn_telemetry_client *client;
    stn_telemetry_client *next;
    time_t now=time(NULL);

    snapshot_read(service,&snapshot);

    memset(&status,0,sizeof(status));
    status.chain_connected=snapshot.chain_connected;
    status.chain_host=snapshot.chain_host;
    status.chain_port=snapshot.chain_port;
    status.work_available=snapshot.work_available;
    status.miners=snapshot.miners;
    status.hashrate=snapshot.hashrate;
    status.job_id=snapshot.job_id;
    status.base_id=snapshot.base_id;
    status.target=snapshot.target;
    status.uptime_seconds=snapshot.uptime_seconds;

    accept_clients(service,now);

    client=service->clients;
    while(client!=NULL){
        next=client->next;
        if(!client_tick(client,&status,now)){
            client_remove(service,client);
        }
        client=next;
    }
}

static void worker_sleep(void)
{
#ifdef _WIN32
    Sleep(STN_TELEMETRY_WORKER_POLL_MS);
#else
    struct timespec request;
    struct timespec remaining;

    request.tv_sec=0;
    request.tv_nsec=(long)STN_TELEMETRY_WORKER_POLL_MS*1000000L;
    while(nanosleep(&request,&remaining)!=0){
        if(errno!=EINTR){return;}
        request=remaining;
    }
#endif
}

#ifdef _WIN32
static DWORD WINAPI telemetry_worker(LPVOID argument)
#else
static void *telemetry_worker(void *argument)
#endif
{
    stn_telemetry_service *service=(stn_telemetry_service *)argument;

    while(atomic_load_explicit(
        &service->worker_running,memory_order_acquire)!=0)
    {
        network_tick(service);
        worker_sleep();
    }

#ifdef _WIN32
    return 0;
#else
    return NULL;
#endif
}

int stn_telemetry_service_open(stn_telemetry_service *service)
{
    stn_telemetry_socket socket_fd;
    struct sockaddr_in address;
    int reuse=1;

    if(service==NULL || service->port==0u || service->worker_started){return 0;}

    socket_fd=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
    if(socket_fd==STN_TELEMETRY_INVALID_SOCKET){return 0;}

#ifdef _WIN32
    if(setsockopt(socket_fd,SOL_SOCKET,SO_REUSEADDR,
        (const char *)&reuse,(int)sizeof(reuse))!=0)
#else
    if(setsockopt(socket_fd,SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof(reuse))!=0)
#endif
    {
        socket_close(socket_fd);
        return 0;
    }

    memset(&address,0,sizeof(address));
    address.sin_family=AF_INET;
    address.sin_addr.s_addr=htonl(INADDR_ANY);
    address.sin_port=htons(service->port);

    if(bind(socket_fd,(const struct sockaddr *)&address,sizeof(address))!=0 ||
       listen(socket_fd,SOMAXCONN)!=0 ||
       !socket_nonblocking(socket_fd))
    {
        socket_close(socket_fd);
        return 0;
    }

    service->listen_socket=socket_fd;
    atomic_store_explicit(&service->worker_running,1,memory_order_release);

#ifdef _WIN32
    service->worker=CreateThread(NULL,0,telemetry_worker,service,0,NULL);
    if(service->worker==NULL){
        atomic_store_explicit(&service->worker_running,0,memory_order_release);
        socket_close(socket_fd);
        service->listen_socket=STN_TELEMETRY_INVALID_SOCKET;
        return 0;
    }
#else
    if(pthread_create(&service->worker,NULL,telemetry_worker,service)!=0){
        atomic_store_explicit(&service->worker_running,0,memory_order_release);
        socket_close(socket_fd);
        service->listen_socket=STN_TELEMETRY_INVALID_SOCKET;
        return 0;
    }
#endif

    service->worker_started=1;
    return 1;
}

void stn_telemetry_service_tick(
    stn_telemetry_service *service,
    const stn_telemetry_status *status,
    time_t now)
{
    (void)now;
    if(service==NULL || status==NULL){return;}
    snapshot_publish(service,status);
}

void stn_telemetry_service_close(stn_telemetry_service *service)
{
    if(service==NULL){return;}

    if(service->worker_started){
        atomic_store_explicit(&service->worker_running,0,memory_order_release);
#ifdef _WIN32
        (void)WaitForSingleObject(service->worker,INFINITE);
        CloseHandle(service->worker);
        service->worker=NULL;
#else
        (void)pthread_join(service->worker,NULL);
#endif
        service->worker_started=0;
    }

    while(service->clients!=NULL){
        client_remove(service,service->clients);
    }

    if(service->listen_socket!=STN_TELEMETRY_INVALID_SOCKET){
        socket_close(service->listen_socket);
        service->listen_socket=STN_TELEMETRY_INVALID_SOCKET;
    }
}

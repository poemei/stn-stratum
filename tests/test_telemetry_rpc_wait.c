#include "stn_rpc_linux.h"
#include "stn_telemetry.h"
#include <assert.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>

static stn_telemetry_service service;
static stn_telemetry_status status={1,"test",18473,1,2,1234,"job","base","target",1};
static void idle(void *user)
{
    assert(user==&service);
    stn_telemetry_service_tick(&service,&status,time(NULL));
}
static int listener(struct sockaddr_in *address)
{
    int fd=socket(AF_INET,SOCK_STREAM,0);socklen_t n=sizeof(*address);
    assert(fd>=0);memset(address,0,sizeof(*address));address->sin_family=AF_INET;
    address->sin_addr.s_addr=htonl(INADDR_LOOPBACK);
    assert(bind(fd,(void *)address,sizeof(*address))==0);
    assert(getsockname(fd,(void *)address,&n)==0&&listen(fd,8)==0);return fd;
}
static void pause_ms(long ms)
{
    struct timespec ts={ms/1000,(ms%1000)*1000000};nanosleep(&ts,NULL);
}
int main(void)
{
    struct sockaddr_in backend_address,web_address;
    int backend=listener(&backend_address),web=listener(&web_address),child_status;
    pid_t chain,observer;stn_rpc_linux transport;
    uint8_t request[24]={0},response[28];size_t length=0;
    alarm(12);
    stn_telemetry_service_init(&service,ntohs(web_address.sin_port));
    service.listen_socket=web;assert(fcntl(web,F_SETFL,O_NONBLOCK)==0);
    chain=fork();assert(chain>=0);
    if(chain==0){
        int fd=accept(backend,NULL,NULL);uint8_t reply[28]={0};
        close(web);assert(fd>=0&&recv(fd,reply,24,MSG_WAITALL)==24);
        memset(reply,0,sizeof(reply));reply[23]=4;memcpy(reply+24,"test",4);
        pause_ms(1500);assert(send(fd,reply,12,0)==12);
        pause_ms(500);assert(send(fd,reply+12,12,0)==12);
        pause_ms(2000);assert(send(fd,reply+24,4,0)==4);close(fd);_exit(0);
    }
    observer=fork();assert(observer>=0);
    if(observer==0){
        int i;close(backend);close(web);pause_ms(100);
        for(i=0;i<10;i++){
            int fd=socket(AF_INET,SOCK_STREAM,0);char reply[4096];size_t used=0;
            struct timeval timeout={0,700000};
            const char get[]="GET /status HTTP/1.1\r\nHost: test\r\n\r\n";
            assert(fd>=0&&setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout))==0);
            assert(connect(fd,(void *)&web_address,sizeof(web_address))==0);
            assert(send(fd,get,sizeof(get)-1,0)==sizeof(get)-1);
            for(;;){ssize_t got=recv(fd,reply+used,sizeof(reply)-1-used,0);
                assert(got>=0);if(got==0){break;}used+=(size_t)got;assert(used<sizeof(reply)-1);}
            reply[used]=0;assert(strstr(reply,"HTTP/1.1 200 OK")&&strstr(reply,"\"hashrate\":1234"));
            close(fd);pause_ms(180);
        }
        _exit(0);
    }
    stn_rpc_linux_init(&transport,"127.0.0.1",ntohs(backend_address.sin_port));
    transport.idle=idle;transport.idle_user=&service;
    assert(stn_rpc_linux_exchange(&transport,request,sizeof(request),response,sizeof(response),&length));
    assert(length==28&&memcmp(response+24,"test",4)==0);
    assert(waitpid(observer,&child_status,0)==observer&&WIFEXITED(child_status)&&WEXITSTATUS(child_status)==0);
    assert(waitpid(chain,&child_status,0)==chain&&WIFEXITED(child_status)&&WEXITSTATUS(child_status)==0);
    stn_rpc_linux_close(&transport);stn_telemetry_service_close(&service);close(backend);
    puts("PASS: ten HTTP status replies during stalled/fragmented Chain header and payload; RPC reply preserved.");
    return 0;
}

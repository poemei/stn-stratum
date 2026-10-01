/* Exercise the real Linux session scheduler with queued and partial frames. */
#include "../src/stn_stratum_server.c"
#include <assert.h>
static stn_stratum_server fixture;
int main(void)
{
    int a[2],b[2],flags;uint8_t request[48]={0},reply[12];
    stn_client_session first={0},second={0};
    assert(socketpair(AF_UNIX,SOCK_STREAM,0,a)==0);
    assert(socketpair(AF_UNIX,SOCK_STREAM,0,b)==0);
    flags=fcntl(a[0],F_GETFL,0);assert(fcntl(a[0],F_SETFL,flags|O_NONBLOCK)==0);
    flags=fcntl(b[0],F_GETFL,0);assert(fcntl(b[0],F_SETFL,flags|O_NONBLOCK)==0);
    first.socket=(uintptr_t)a[0];first.next=&second;first.address_registered=1;
    second.socket=(uintptr_t)b[0];second.address_registered=1;fixture.clients=&first;
    memcpy(request,"STNM",4);request[4]=1;request[5]=STN_MINER_SUBMIT;
    assert(send(a[1],request,48,0)==48);assert(send(a[1],request,48,0)==48);
    assert(send(b[1],request,48,0)==48);
    service_clients(&fixture);
    assert(recv(a[1],reply,12,MSG_DONTWAIT)==12&&reply[11]==STN_MINER_RESULT_STALE);
    assert(recv(a[1],reply,12,MSG_DONTWAIT)==-1&&errno==EAGAIN);
    assert(recv(b[1],reply,12,MSG_DONTWAIT)==12&&reply[11]==STN_MINER_RESULT_STALE);
    service_clients(&fixture);assert(recv(a[1],reply,12,MSG_DONTWAIT)==12);
    assert(send(a[1],request,4,0)==4);assert(send(b[1],request,48,0)==48);
    service_clients(&fixture);assert(first.rx_used==4);
    assert(recv(b[1],reply,12,MSG_DONTWAIT)==12);
    assert(send(a[1],request+4,44,0)==44);service_clients(&fixture);
    assert(first.rx_used==0&&recv(a[1],reply,12,MSG_DONTWAIT)==12);
    close(a[0]);close(a[1]);close(b[0]);close(b[1]);
    puts("Session fairness: queued miner yields, peer replies, partial frames preserved. PASS");return 0;
}

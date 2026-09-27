/* Exercise the real platform refresh path without a live Chain or miner. */
#include "../src/stn_stratum_server.c"
static stn_stratum_server fixture;
static int disconnected;
static uint16_t response_code=5u;
static int exchange_status(void *user,const uint8_t *q,size_t n,
    uint8_t *r,size_t capacity,size_t *written)
{
    (void)user;
    if(disconnected || n!=24 || capacity<24){return 0;}
    memcpy(r,q,24);r[7]=2;
    r[10]=(uint8_t)(response_code>>8);r[11]=(uint8_t)response_code;
    memset(r+20,0,4);*written=24;return 1;
}
int main(void)
{
    unsigned i;
    stn_rpc_client_init(&fixture.chain_rpc,NULL,exchange_status);
    fixture.active_chain_server=7;

    /* Reachable Chain without replacement work must retain current work. */
    response_code=5u;
    for(i=0;i<3;++i){
        fixture.have_work=1;
        fixture.active_template_length=100;
        refresh_work(&fixture);
        if(!fixture.chain_available || !fixture.have_work ||
           fixture.active_template_length!=100 || fixture.active_chain_server!=7 ||
           fixture.chain_rpc.exchange!=exchange_status){return 1;}
    }

    /* RPC CAPACITY during MINING_TEMPLATE is also transient replacement-work
     * unavailability. It must not retire the current immutable Work ID. */
    response_code=9u;
    fixture.have_work=1;
    fixture.active_template_length=100;
    refresh_work(&fixture);
    if(!fixture.chain_available || !fixture.have_work ||
       fixture.active_template_length!=100 || fixture.active_chain_server!=7){return 1;}

    disconnected=1;
    refresh_work(&fixture);
    if(fixture.chain_available || fixture.have_work){return 1;}
    puts("Work availability: reachable unavailable/capacity retained work; transport failure marked offline. PASS");
    return 0;
}

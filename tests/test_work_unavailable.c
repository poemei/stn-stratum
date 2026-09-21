/* Exercise the real platform refresh path without a live Chain or miner. */
#include "../src/stn_stratum_server.c"
static stn_stratum_server fixture;
static int disconnected;
static int exchange_unavailable(void *user,const uint8_t *q,size_t n,
    uint8_t *r,size_t capacity,size_t *written)
{
    (void)user;
    if(disconnected || n!=24 || capacity<24){return 0;}
    memcpy(r,q,24);r[7]=2;r[10]=0;r[11]=5;
    memset(r+20,0,4);*written=24;return 1;
}
int main(void)
{
    unsigned i;
    stn_rpc_client_init(&fixture.chain_rpc,NULL,exchange_unavailable);
    fixture.active_chain_server=7;
    for(i=0;i<3;++i){
        fixture.have_work=1;
        fixture.active_template_length=100;
        refresh_work(&fixture);
        if(!fixture.chain_available || fixture.have_work ||
           fixture.active_template_length!=0 || fixture.active_chain_server!=7 ||
           fixture.chain_rpc.exchange!=exchange_unavailable){return 1;}
    }
    disconnected=1;
    refresh_work(&fixture);
    if(fixture.chain_available || fixture.have_work){return 1;}
    puts("Work availability: 3 reachable-unavailable polls retained endpoint; transport failure marked offline. PASS");
    return 0;
}

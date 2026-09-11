/* Actual Stratum client/transport contract driver. No miner or consensus code. */
#include "stn_rpc_client.h"
#include "stn_rpc_win32.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static uint8_t work[STN_RPC_CLIENT_MAX_PAYLOAD],reply[STN_RPC_CLIENT_MAX_PAYLOAD];
static size_t work_length;
static void put(uint8_t *p,size_t n,uint64_t v){while(n){p[--n]=(uint8_t)v;v>>=8;}}
static int fault;
static int mock(void *u,const uint8_t *q,size_t qn,uint8_t *r,size_t cap,size_t *rn)
{
    size_t n=184;(void)u;(void)qn;
    if(cap<208){return 0;}memcpy(r,q,24);r[7]=2;memset(r+24,0,n);
    if(fault>=10){n=0;r[11]=(uint8_t)(fault-10);}
    put(r+20,4,n);*rn=24+n;
    if(fault>=22){
        n=fault>=26 ? (fault==26 ? 79u : 80u) : 432u;
        memset(r+24,0,n);r[11]=0;put(r+20,4,n);*rn=24+n;
        if(fault<26){put(r+24+64,4,364);memcpy(r+24+68,"STNB",4);r[24+73]=3;
            if(fault==22){put(r+20,4,431);*rn=455;}
            if(fault==23){r[24+67]=0;}
            if(fault==24){r[24+73]=2;}}
        return 1;
    }
    switch(fault){case 1:r[0]=0;break;case 2:r[5]=1;break;case 3:r[7]=1;break;
    case 4:r[9]^=1;break;case 5:r[19]^=1;break;case 6:*rn=23;break;
    case 7:put(r+20,4,175);*rn=199;break;case 8:*rn=cap+1;break;
    case 9:r[11]=7;break;default:break;}return 1;
}
static int selftest(void)
{
    stn_rpc_client c;size_t n;int i,fail=0;stn_rpc_client_code code;
    stn_rpc_client_init(&c,NULL,mock);
    for(i=0;i<=20;i++){
        fault=i;code=stn_rpc_client_info(&c,reply,sizeof(reply),&n);
        if(i==0){if(code!=STN_RPC_CLIENT_OK || n!=184){++fail;}}
        else if(i<10 || i==10){if(code!=STN_RPC_CLIENT_INVALID || n!=0){++fail;}}
        else if(code!=(stn_rpc_client_code)(i-10) || n!=0){++fail;}
    }
    for(i=22;i<=27;i++){
        fault=i;
        code=i<26 ? stn_rpc_client_mining_template(&c,reply,sizeof(reply),&n) :
            stn_rpc_client_submit_work(&c,work,432,reply,sizeof(reply),&n);
        if(i==25 || i==27){if(code!=STN_RPC_CLIENT_OK){++fail;}}
        else if(code!=STN_RPC_CLIENT_INVALID || n!=0){++fail;}
    }
    printf("Stratum interface parser: 27 checks, %d failures.\n",fail);return fail!=0;
}
int main(int argc,char **argv)
{
    stn_rpc_client client;stn_rpc_win32 transport;char command[80];size_t n,i;
    stn_rpc_client_code code;
    if(argc==2 && strcmp(argv[1],"--selftest")==0){return selftest();}
    if(argc!=2){return 2;}
    stn_rpc_win32_init(&transport,"127.0.0.1",(uint16_t)strtoul(argv[1],NULL,10));
    stn_rpc_client_init(&client,&transport,stn_rpc_win32_exchange);
    while(fgets(command,sizeof(command),stdin)!=NULL){
        if(strncmp(command,"quit",4)==0){break;}
        n=0;code=STN_RPC_CLIENT_INVALID;
        if(strncmp(command,"info",4)==0){code=stn_rpc_client_info(&client,reply,sizeof(reply),&n);}
        else if(strncmp(command,"template",8)==0){
            code=stn_rpc_client_mining_template(&client,reply,sizeof(reply),&n);
            if(code==STN_RPC_CLIENT_OK){memcpy(work,reply,n);work_length=n;}
        }else if(strncmp(command,"submit ",7)==0 && work_length>=432){
            uint64_t nonce=strtoull(command+7,NULL,10);put(work+220,8,nonce);
            code=stn_rpc_client_submit_work(&client,work,work_length,reply,sizeof(reply),&n);
        }else if(strncmp(command,"unknown",7)==0){
            uint8_t q[24]={0};size_t rn;
            memcpy(q,"STNC",4);q[5]=STN_RPC_CLIENT_VERSION;q[7]=1;q[8]=0x77;q[9]=0x77;q[19]=123;
            if(stn_rpc_win32_exchange(&transport,q,24,reply,sizeof(reply),&rn) && rn==24){code=(stn_rpc_client_code)reply[11];}
            else{code=STN_RPC_CLIENT_TRANSPORT;}
        }
        printf("RESULT %d %zu ",(int)code,n);for(i=0;i<n;++i){printf("%02X",reply[i]);}putchar('\n');fflush(stdout);
    }
    stn_rpc_win32_close(&transport);return 0;
}

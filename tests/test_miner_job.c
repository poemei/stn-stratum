#include <stdio.h>
#include <string.h>
#include "stn_miner_job.h"
static unsigned n,f;
#define C(x) do{++n;if(!(x)){++f;printf("FAIL line %d: %s\n",__LINE__,#x);}}while(0)
static uint32_t r32(const uint8_t *p){return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];}
static int max_target(const uint8_t *p){
 size_t i;
 if(p[0]!=0x7fu)return 0;
 for(i=1u;i<32u;++i)if(p[i]!=0xffu)return 0;
 return 1;
}
int main(void){
 uint8_t t[68u+168u]={0},h[STN_MINER_JOB_HEADER_SIZE];uint32_t len=0;size_t i;
 memcpy(t+32u,"01234567890123456789012345678901",32u);
 t[68u+STN_MINER_CHAIN_TARGET_OFFSET+31u]=1u;
 for(i=0;i<8u;++i)t[68u+STN_MINER_CHAIN_NONCE_OFFSET+i]=(uint8_t)(i+1u);
 C(stn_miner_job_build(t,sizeof(t),h,&len)==1);
 C(len==168u);C(memcmp(h,STN_MINER_MAGIC,4u)==0);C(h[4]==STN_MINER_VERSION);C(h[5]==STN_MINER_JOB);C(h[6]==0u&&h[7]==0u);
 C(memcmp(h+8u,t+32u,32u)==0);C(r32(h+STN_MINER_JOB_BLOCK_LENGTH_OFFSET)==168u);
 C(memcmp(h+STN_MINER_JOB_INITIAL_NONCE_OFFSET,t+68u+STN_MINER_CHAIN_NONCE_OFFSET,8u)==0);
 C(memcmp(h+STN_MINER_JOB_CHAIN_TARGET_OFFSET,t+68u+STN_MINER_CHAIN_TARGET_OFFSET,32u)==0);
 C(h[STN_MINER_JOB_SHARE_TARGET_OFFSET+31u]==10u);

 /* Product sets bit 255 without a 256-bit carry: saturate to 2^255-1. */
 memset(t+68u+STN_MINER_CHAIN_TARGET_OFFSET,0,32u);
 t[68u+STN_MINER_CHAIN_TARGET_OFFSET]=0x0du;
 C(stn_miner_job_build(t,sizeof(t),h,&len)==1);
 C(max_target(h+STN_MINER_JOB_SHARE_TARGET_OFFSET));

 /* Product overflows 256 bits: saturate to the same protocol maximum. */
 memset(t+68u+STN_MINER_CHAIN_TARGET_OFFSET,0xff,32u);
 t[68u+STN_MINER_CHAIN_TARGET_OFFSET]=0x7fu;
 C(stn_miner_job_build(t,sizeof(t),h,&len)==1);
 C(max_target(h+STN_MINER_JOB_SHARE_TARGET_OFFSET));

 C(stn_miner_job_build(t,67u,h,&len)==0);
 printf("Stratum JOB framing: %u checks, %u failures.\n",n,f);return f?1:0;
}

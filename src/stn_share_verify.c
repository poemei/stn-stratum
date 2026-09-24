#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "stn_share_verify.h"
#define H 168u
#define N 152u
static const uint8_t domain[]="STN-CHAIN:BLOCK:ID:1";
typedef struct {uint32_t s[8];uint64_t bits;uint8_t b[64];size_t n;} ctx;
static uint32_t rr(uint32_t v,uint32_t n){return(v>>n)|(v<<(32u-n));}
static uint32_t r32(const uint8_t*p){return((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];}
static void w32(uint8_t*p,uint32_t v){p[0]=(uint8_t)(v>>24);p[1]=(uint8_t)(v>>16);p[2]=(uint8_t)(v>>8);p[3]=(uint8_t)v;}
static void tr(ctx*c,const uint8_t*b){static const uint32_t k[64]={0x428a2f98u,0x71374491u,0xb5c0fbcfu,0xe9b5dba5u,0x3956c25bu,0x59f111f1u,0x923f82a4u,0xab1c5ed5u,0xd807aa98u,0x12835b01u,0x243185beu,0x550c7dc3u,0x72be5d74u,0x80deb1feu,0x9bdc06a7u,0xc19bf174u,0xe49b69c1u,0xefbe4786u,0x0fc19dc6u,0x240ca1ccu,0x2de92c6fu,0x4a7484aau,0x5cb0a9dcu,0x76f988dau,0x983e5152u,0xa831c66du,0xb00327c8u,0xbf597fc7u,0xc6e00bf3u,0xd5a79147u,0x06ca6351u,0x14292967u,0x27b70a85u,0x2e1b2138u,0x4d2c6dfcu,0x53380d13u,0x650a7354u,0x766a0abbu,0x81c2c92eu,0x92722c85u,0xa2bfe8a1u,0xa81a664bu,0xc24b8b70u,0xc76c51a3u,0xd192e819u,0xd6990624u,0xf40e3585u,0x106aa070u,0x19a4c116u,0x1e376c08u,0x2748774cu,0x34b0bcb5u,0x391c0cb3u,0x4ed8aa4au,0x5b9cca4fu,0x682e6ff3u,0x748f82eeu,0x78a5636fu,0x84c87814u,0x8cc70208u,0x90befffau,0xa4506cebu,0xbef9a3f7u,0xc67178f2u};uint32_t w[64],a,bv,cv,d,e,f,g,h,x,y;size_t i;for(i=0;i<16;i++)w[i]=r32(b+i*4);for(i=16;i<64;i++){x=rr(w[i-15],7)^rr(w[i-15],18)^(w[i-15]>>3);y=rr(w[i-2],17)^rr(w[i-2],19)^(w[i-2]>>10);w[i]=w[i-16]+x+w[i-7]+y;}a=c->s[0];bv=c->s[1];cv=c->s[2];d=c->s[3];e=c->s[4];f=c->s[5];g=c->s[6];h=c->s[7];for(i=0;i<64;i++){uint32_t s1=rr(e,6)^rr(e,11)^rr(e,25),ch=(e&f)^((~e)&g),t1=h+s1+ch+k[i]+w[i],s0=rr(a,2)^rr(a,13)^rr(a,22),maj=(a&bv)^(a&cv)^(bv&cv),t2=s0+maj;h=g;g=f;f=e;e=d+t1;d=cv;cv=bv;bv=a;a=t1+t2;}c->s[0]+=a;c->s[1]+=bv;c->s[2]+=cv;c->s[3]+=d;c->s[4]+=e;c->s[5]+=f;c->s[6]+=g;c->s[7]+=h;}
static void init(ctx*c){static const uint32_t s[8]={0x6a09e667u,0xbb67ae85u,0x3c6ef372u,0xa54ff53au,0x510e527fu,0x9b05688cu,0x1f83d9abu,0x5be0cd19u};memcpy(c->s,s,sizeof(s));c->bits=0;c->n=0;}
static void up(ctx*c,const uint8_t*p,size_t n){while(n){size_t q=64u-c->n;if(q>n)q=n;memcpy(c->b+c->n,p,q);c->n+=q;p+=q;n-=q;c->bits+=(uint64_t)q*8u;if(c->n==64u){tr(c,c->b);c->n=0;}}}
static void fin(ctx*c,uint8_t*d){uint64_t bits=c->bits;size_t i;c->b[c->n++]=0x80u;if(c->n>56u){while(c->n<64u)c->b[c->n++]=0;tr(c,c->b);c->n=0;}while(c->n<56u)c->b[c->n++]=0;for(i=0;i<8;i++)c->b[63u-i]=(uint8_t)(bits>>(i*8u));tr(c,c->b);for(i=0;i<8;i++)w32(d+i*4,c->s[i]);}
static int le(const uint8_t*a,const uint8_t*b){size_t i;for(i=0;i<32;i++){if(a[i]<b[i])return 1;if(a[i]>b[i])return 0;}return 1;}
stn_share_class stn_share_classify(const uint8_t*block,size_t length,uint64_t nonce,const uint8_t share[32],const uint8_t chain[32],uint8_t digest[32]){uint8_t header[H],hash[32];ctx c;size_t i;if(!block||!share||!chain||!digest||length<H)return STN_SHARE_INVALID;memcpy(header,block,H);for(i=0;i<8;i++){header[N+7u-i]=(uint8_t)(nonce&0xffu);nonce>>=8;}init(&c);up(&c,domain,sizeof(domain));up(&c,header,sizeof(header));fin(&c,hash);if(!le(hash,share))return STN_SHARE_INVALID;memcpy(digest,hash,32);return le(hash,chain)?STN_SHARE_BLOCK:STN_SHARE_QUALIFYING;}

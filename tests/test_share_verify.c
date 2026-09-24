#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "stn_share_verify.h"
int main(void){uint8_t b[168]={0},s[32],c[32],h[32];unsigned n=0,f=0;memset(s,0xff,32);memset(c,0,32);c[31]=1;n++;if(stn_share_classify(b,sizeof(b),0,s,c,h)==STN_SHARE_INVALID)f++;n++;if(stn_share_classify(NULL,sizeof(b),0,s,c,h)!=STN_SHARE_INVALID)f++;memset(s,0,32);n++;if(stn_share_classify(b,sizeof(b),0,s,c,h)!=STN_SHARE_INVALID)f++;printf("Stratum share classification: %u checks, %u failures.\n",n,f);return f?1:0;}

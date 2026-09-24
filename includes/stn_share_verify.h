#ifndef STN_SHARE_VERIFY_H
#define STN_SHARE_VERIFY_H
#include <stddef.h>
#include <stdint.h>
typedef enum stn_share_class { STN_SHARE_INVALID=0, STN_SHARE_QUALIFYING=1, STN_SHARE_BLOCK=2 } stn_share_class;
stn_share_class stn_share_classify(const uint8_t *canonical_block,size_t block_length,uint64_t nonce,const uint8_t share_target[32],const uint8_t chain_target[32],uint8_t digest[32]);
#endif

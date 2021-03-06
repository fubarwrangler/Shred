#ifndef RC4_H_
#define RC4_H_

#include <stdio.h>
#include <stdlib.h>

struct rc4_ctx {
    unsigned char S[256];
};

void rc4_init_key(struct rc4_ctx *ctx, unsigned char *key, size_t klen);
void rc4_fill_buf(struct rc4_ctx *ctx, unsigned char *buf, size_t nb);
void rc4_xor_stream(struct rc4_ctx *ctx, unsigned char *buf, size_t n);
void rc4_shuffle_key(struct rc4_ctx *ctx, unsigned char *k, size_t l);
struct rc4_ctx *rc4_copy_ctx(struct rc4_ctx *src);

#endif

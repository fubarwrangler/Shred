#include <stdio.h>
#include <stdlib.h>

#include "rc4.h"

void rc4_init_key(struct rc4_ctx *ctx, unsigned char *key, size_t klen)
{
    int i;
    unsigned char j = 0, tmp;

    for(i = 0; i < 256; i++)
        ctx->S[i] = i;

    for(i = 0; i < 256; i++)    {
        j = (j + ctx->S[i] + key[i % klen]) & 255;

        tmp = ctx->S[i];
        ctx->S[i] = ctx->S[j];
        ctx->S[j] = tmp;
    }
}

void rc4_fill_buf(struct rc4_ctx *ctx, unsigned char *buf, size_t nb)
{
    unsigned char i, j, k, idx, tmp;
    unsigned char *state = ctx->S;
    size_t n = 0;

    i = j = k = 0;

    do
    {
        i = (i + 1) & 255;
        j = (j + state[i]) & 255;

        tmp = state[i];
        state[i] = state[j];
        state[j] = tmp;

        idx = (state[i] + state[j]) & 255;

        *(buf + n) = state[idx];
    } while(++n < nb);
}

#ifdef TEST
#include <string.h>

int main(int argc, char *argv[])
{
    struct rc4_ctx ctx;
    char buf[1024];
    int n = 24323424;

    if(argc < 2)    {
        puts("Error, requires key argument\n");
        return 1;
    }
    rc4_init_key(&ctx, argv[1], strlen(argv[1]));
    while(n--)  {
        rc4_fill_buf(&ctx, buf, 1024);
        fwrite(buf, 1024, 1, stdout);
    }

    return 0;
}

#endif
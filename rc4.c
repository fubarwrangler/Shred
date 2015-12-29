#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rc4.h"

/* Initalize the RC4 state @ctx with @key of length @klen */
void rc4_init_key(struct rc4_ctx *ctx, unsigned char *key, size_t klen)
{
	int i;

	for(i = 0; i < 256; i++)
		ctx->S[i] = i;

	rc4_shuffle_key(ctx, key, klen);
}

/* Second half of the key-init algorithm, used to preserve state from
 * last call (ctx doesn't get filled 1-256 in order)
 */
void rc4_shuffle_key(struct rc4_ctx *ctx, unsigned char *k, size_t l)
{
	int i;
	unsigned char tmp, j = 0;

	for(i = 0; i < 256; i++)    {
		j = (j + ctx->S[i] + k[i % l]) & 255;

		tmp = ctx->S[i];
		ctx->S[i] = ctx->S[j];
		ctx->S[j] = tmp;
	}
}

/* Write @nb bytes of RC4 keystream to @buf from cipher context @ctx */
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

/* XOR a buffer with the keystream */
void rc4_xor_stream(struct rc4_ctx *ctx, unsigned char *buf, size_t n)
{
	unsigned char i, j, k, tmp;
	unsigned char *state = ctx->S;
	size_t ctr = 0;

	i = j = k = 0;

	do
	{
		i = (i + 1) & 255;
		j = (j + state[i]) & 255;

		tmp = state[i];
		state[i] = state[j];
		state[j] = tmp;

		*(buf + ctr) ^= state[(state[i] + state[j]) & 255];
	} while(++ctr < n);
}

/* Copy an existing rc4 context */
struct rc4_ctx *rc4_copy_ctx(struct rc4_ctx *src)
{
	struct rc4_ctx *new_ctx = malloc(sizeof(struct rc4_ctx));
	if (new_ctx == NULL)
		return NULL;
	memcpy(new_ctx, src, sizeof(struct rc4_ctx));
	return new_ctx;
}


#ifdef TEST
#include <string.h>

int main(int argc, char *argv[])
{
	struct rc4_ctx ctx;
	char buf[1024];
	int n = 456;

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

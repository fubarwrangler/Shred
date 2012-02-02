#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include "rc4.h"

void random_bytes(unsigned char *buf, size_t len)
{
	int fd;
	fd = open("/dev/urandom", O_RDONLY);
	if (fd >= 0)
	{
		size_t rb = 0;
		while(rb < len)	{
			rb += read(fd, buf + rb, len - rb);
		}
		close(fd);
	}
}

#define BUF_SIZE 4096
#define REPS (8192 << 3)

int main(int argc, char *argv[])
{
	unsigned char key[32];
	unsigned char data[BUF_SIZE];
	struct rc4_ctx ctx;

	if(data != NULL)	{
		do {
			int n;

			random_bytes(key, 32);
			rc4_init_key(&ctx, key, 32);
			for(n = 0; n < REPS; n++)	{
				rc4_fill_buf(&ctx, data, BUF_SIZE);
				fwrite(data, BUF_SIZE, 1, stdout);
			}
		} while(1);
	}
	return 0;
}
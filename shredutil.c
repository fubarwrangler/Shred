#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

/* Read @len random bytes from /dev/urandom into @buf */
void read_random_bytes(const char *rand_device, unsigned char *buf, size_t len)
{
	int fd;
	fd = open(rand_device, O_RDONLY);
	if (fd >= 0) {
		size_t rb = 0;
		while(rb < len)	{
			ssize_t r = read(fd, buf + rb, len - rb);
			if(r < 0)	{
				perror("Read random");
				exit(EXIT_FAILURE);
			}
			rb += r;
		}
		close(fd);
	} else {
		perror("Random device");
		exit(EXIT_FAILURE);
	}
}

/* Write @len bytes from @buf out to file descriptor @fd.
 * Return 1 on success, 0 on full device, exit on failure
 */
int write_block(int fd, unsigned char *buf, size_t len)
{
	size_t written = 0;
	ssize_t this_write = 0;

	while(written < len)	{

		this_write = write(fd, buf + written, len - written);

		if(errno || this_write < 0)	{
			if(errno == ENOSPC)	{
				fputs("\nNo space left, exiting", stderr);
				return 0;
			}
			perror("Writing data");
			exit(EXIT_FAILURE);
		}

		written += this_write;
	}
	return 1;
}

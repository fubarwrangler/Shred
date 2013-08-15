#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

#include "cmdlineparse.h"

char     *file = NULL;
uint64_t skip_beginning = 0;
uint64_t stride_size = 0;
uint64_t read_size = 4096;
bool     verbose = false;

uint64_t filesize = 0;


/* Set the configuration options above from cmdline */
static void initialize_options(int argc, char *argv[])
{
	int c;

	while((c=getopt(argc, argv, "+hs:l:n:")) != -1) {
		switch(c)   {
			case 's':
				skip_beginning = parse_num(c);
				break;
			case 'l':
				stride_size = parse_num(c);
				break;
			case 'n':
				read_size = parse_num(c);
				break;
			case 'h':
				fprintf(stderr,
"Usage: %s [options] FILE \n\
	FILE	file or device to read\n\
  Options:\n\
	-s  bytes to skip before starting (default %lu)\n\
	-l  length of stride to take between reads, in bytes (default %lu)\n\
	-n  number of bytes to read at each stride (default %lu)\n\
  Notes:\n\
	Integer values can be postfixed with a multiplier, one of the\n\
	following letters:\n\
	  k/K m/M g/G\n\
	for kilo, mega, or giga-byte. The lower-case versions return the power\n\
	of two nearest (1k = 1024), and the upper-case returns an exact power of\n\
	ten (1K = 1000).\n\
", argv[0], skip_beginning, stride_size, read_size);
				exit(EXIT_SUCCESS);
			case '?':
				if(strchr("hsln", optopt) == NULL)
					fprintf(stderr,
						"Unknown option -%c encountered\n", optopt);
				else
					fprintf(stderr,
						"Option -%c requires an argument\n", optopt);
				exit(EXIT_FAILURE);
			default:
				abort();
		}
	}

	if(optind + 1 == argc)	{
		file = argv[optind];
		return;
	} else if (optind + 1 < argc) {
		fprintf(stderr, "Error, extra arguments found starting with: %s\n", 
				argv[optind + 1]);
	} else {
		fprintf(stderr, "Error, filename argument is required, "
						"run with -h to see options\n");
	}
	exit(EXIT_FAILURE);
}

static inline uint64_t do_seek(int fd, uint64_t offset, int where)
{
	off_t n = lseek(fd, offset, where);
	if(n < 0)	{
		if(errno == EINVAL)	{
			return 0;
		}
		perror("lseek()");
		exit(EXIT_FAILURE);
	}
	return n;
}

static void write_buf(unsigned char *buf, size_t len)
{
	size_t wb = 0;
	ssize_t written;

	while(wb < len)	{
		if((written = write(STDOUT_FILENO, buf + wb, len - wb)) < 0)	{
			perror("read");
			exit(EXIT_FAILURE);	
		}
		wb += written;
	}
}


static int read_buf(int fd, unsigned char *buf, size_t len)
{
	size_t rb = 0;
	ssize_t this_read;

	while(rb < len)	{
		if((this_read = read(fd, buf + rb, len - rb)) < 0)	{
			perror("read");
			exit(EXIT_FAILURE);	
		} else if (this_read == 0)	{
			return rb;
		}
		rb += this_read;
	}
	return rb;
}


#define MIN(a, b) (a) < (b) ? (a) : (b)

int main(int argc, char *argv[])
{
	unsigned char buf[8192];
	size_t buf_read;
	uint64_t pos;
	int fd;

	initialize_options(argc, argv);

	//posix_fadvise(fd, skip_beginning, 
	
	if((fd = open(file, O_RDONLY)) < 0)	{
		perror("open");
		return 1;
	}

	filesize = do_seek(fd, 0, SEEK_END);
	pos = do_seek(fd, 0, SEEK_SET);

	printf("%lu\n", filesize);
	fflush(stdout);

	buf_read = MIN(sizeof(buf), read_size);

	if(skip_beginning > 0)
		do_seek(fd, skip_beginning, SEEK_SET);

	while(pos + stride_size < filesize)	{
		size_t total_read = 0;
		size_t bytes;

		while(total_read < read_size)	{
			size_t to_read = MIN(read_size - total_read, buf_read);

			bytes = read_buf(fd, buf, to_read);

			total_read += bytes;
			write_buf(buf, bytes);
			if(bytes < to_read)	{
				//fprintf(stderr, "short read: %lu bytes", bytes);
				return 0;
			}
		}

		if(stride_size > 0)	{
			pos = do_seek(fd, stride_size, SEEK_CUR);
		}
	}

	close(fd);

	return 0;
}

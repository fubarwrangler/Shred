#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include "cmdlineparse.h"

char     *file = NULL;
uint64_t skip_beginning = 0;
uint64_t stride_size = 0;
uint64_t read_size = 4096;
bool     verbose = false;


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

	/* TODO: read filename */
}

int main(int argc, char *argv[])
{
	unsigned char buf[8092];
	int fd;

	initialize_options(argc, argv);

	posix_fadvise(fd, skip_beginning, 

	return 0;
}

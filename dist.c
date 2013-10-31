/*************************************************************************
 * dist.c -- print a histogram of the distribution of characters in the 
 *           incoming stream (or file passed as argv[1]) to do a very
 *           cursory, crappy, and insecure check for randomness!
 *
 ************************************************************************/
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <unistd.h>

#include "cmdlineparse.h"

enum {
	BIT = 1,
	HEX = 4,
	BYTE = 8,
};

uint64_t arr[256] = {0};
char bitcount = BYTE;
char *file = NULL;

/* Set the configuration options above from cmdline */
static void initialize_options(int argc, char *argv[])
{
	int c;

	while((c=getopt(argc, argv, "+hb:")) != -1) {
		switch(c)   {
			case 'b':
				switch(bitcount = parse_num(c))	{
					case 1:
						bitcount = BIT;
						break;
					case 4:
						bitcount = HEX;
						break;
					case 8:
						bitcount = BYTE;
						break;
					default:
						fprintf(stderr, "Error, bit count needs to be '1', '4', or '8'\n");
						exit(EXIT_FAILURE);
				}
				break;
			case 'h':
				fprintf(stderr,
"Usage: %s [options] [FILE] \n\
	FILE	file or device to read (default is stdin)\n\
  Options:\n\
	-b  Bit-length, valid values are 1, 4, and 8\n\
", argv[0]);
				exit(EXIT_SUCCESS);
			case '?':
				if(strchr("bh", optopt) == NULL)
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
		exit(EXIT_FAILURE);
	}
}

int main(int argc, char *argv[])
{
	FILE *fp = stdin;
	unsigned char buf[4096];
	uint64_t total = 0;
	uint64_t norm = 0;
	uint64_t max = 0;
	size_t n, i;

	initialize_options(argc, argv);

	if(file != NULL)	{
		if((fp = fopen(argv[1], "r")) == NULL)	{
			fprintf(stderr, "Error opening %s\n", argv[1]);
			return 1;
		}
	}

	while((n = fread(buf, 1, sizeof(buf), fp)) != 0)	{
		for(i = 0; i < n; i++)
			arr[buf[i]]++;
		total += n;
	}

	if(ferror(fp))	{
		fprintf(stderr, "File error occured\n");
		return 1;
	}

	fclose(fp);

	for(i = 0; i < 256; i++)	{
		if(arr[i] > max)
			max = arr[i];
		norm += arr[i];
	}

	if(max == 0)
		return 1;

	for(int j = 0; j < (1 << bitcount); j++)	{
		int stat;
		stat = (60 * arr[j]) / max;
		printf("%-2x (%7.3f%%) |", j, 100.0f * (double)arr[j] / (double)norm);
		for(int k = 0; k < stat; k++)	{
			putchar('*');
		}
		printf("\n");
	}
	printf("%lu bytes read total\n", total);

	return 0;
}

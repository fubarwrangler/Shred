#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include "cmdlineparse.h"

uint64_t ipow(unsigned int a, unsigned int b)
{
	uint64_t ans = 1;

	while(b--)
		ans *= (uint64_t)a;

	return ans;
}

/* Get a positive integer out of the current optarg (option @c) or exit */
uint64_t parse_num(int c)
{
	char *p;
	long int n;
	unsigned long int mult;

	errno = 0;
	n = strtol(optarg, &p, 10);
	if(errno != 0 || n < 0)	{
		fprintf(stderr, "Error -%c requires a positive integer\n", c);
		exit(EXIT_FAILURE);
	}

	switch(*p)	{
		case '\0':
			return n;
		case 'K':
			mult = ipow(10, 3); break;
		case 'k':
			mult = ipow(2, 10); break;
		case 'M':
			mult = ipow(10, 6); break;
		case 'm':
			mult = ipow(2, 20); break;
		case 'G':
			mult = ipow(10, 9); break;
		case 'g':
			mult = ipow(2, 30); break;
			break;
		default:
			fprintf(stderr,
				"Invalid multiplier character found for -%c: %c, "
				"must be K/k/M/m/G/g\n", c, *p);
			exit(EXIT_FAILURE);
	}

	if(*(p + 1) == '\0' || (*(p + 1) == 'b' && *(p + 2) == '\0'))
		return n * mult;

	fprintf(stderr, "Invalid character found after multiplier for"
					" -%c: %c\n", c, *(p+1));

	exit(EXIT_FAILURE);
}

double parse_dbl(int c)
{
	char *p;
	double d;

	errno = 0;
	d = strtod(optarg, &p);
	if(errno != 0 || d < 0)	{
		fprintf(stderr, "Error -%c requires a (possibly float) num of seconds\n", c);
		exit(EXIT_FAILURE);
	}
	return d;
}


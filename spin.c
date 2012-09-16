/****************************************************************************
 * shred.c -- A program for turning a little bit of good random data
 *			  into a lot of good-enough random data.
 *
 *	Takes a few bytes at a time from /dev/urandom and initializes an RC4
 *	cipher context, which then is used to generate a random key-stream for
 *	a while.  After a certain number of repetitions more random bytes are
 *	read and the key-stream is reinitialized with the new key.
 *
 *	See: http://en.wikipedia.org/wiki/Rc4 for details as to the RC4
 *	algorithm used.  This was chosen because it is considered one of the
 *	fastest nontrivial ciphers to implement in software.
 *
 *	Should use very little CPU, suggested usage is for disk shredding like
 *		"./shred | dd of=/dev/sdd bs=1M"
 *	in parallel for each disk being shredded
 *
 ***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdbool.h>
#include <signal.h>
#include <math.h>
#include <sys/time.h>

#include "rc4.h"

double total_time = -1.0;
size_t total_ram = (1 << 22);

/* If sigint, set done=1 and break out of main loop cleanly */
static void sigint_handler(int signum)
{
	exit(0);
}

/* Setup signal handler */
static void setup_signals(void)
{
	struct sigaction new_action;
	sigset_t self;

	sigemptyset(&self);
	sigaddset(&self, SIGVTALRM);
	new_action.sa_handler = sigint_handler;
	new_action.sa_mask = self;
	new_action.sa_flags = 0;

	sigaction(SIGVTALRM, &new_action, NULL);
}


unsigned long int ipow(unsigned int a, unsigned int b)
{
	unsigned long int ans = 1;

	while(b--)
		ans *= (unsigned long int)a;

	return ans;
}

/* Get a positive integer out of the current optarg (option @c) or exit */
static unsigned long int parse_num(int c)
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

static double parse_dbl(int c)
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

/* Set the configuration options above from cmdline */
static void initialize_options(int argc, char *argv[])
{
	int c;

	while((c=getopt(argc, argv, "+hn:t:")) != -1)	{
		switch(c)	{
			case 'n':
				total_ram = parse_num(c);
				break;
			case 't':
				total_time = parse_dbl(c);
				break;
			case 'h':
				fprintf(stderr,
"Usage: %s [OPTION] [DESTINATION]\n\
  Options:\n\
    -n  amount of RAM to allocate (defaults to 4Mb)\n\
    -t  how long to run in seconds, decimals accepted (forever if not given)\n\
  Notes:\n\
    Integer values can be postfixed with a multiplier, one of the\n\
    following letters:\n\
      k/K m/M g/G\n\
    for kilo, mega, or giga-byte. The lower-case versions return the power\n\
    of two nearest (1k = 1024), and the upper-case returns an exact power of\n\
    ten (1K = 1000).\n\
", argv[0]);
				exit(EXIT_SUCCESS);
			case '?':
				if(strchr("nt", optopt) == NULL)
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
}

static void set_timer(double secs)
{
	struct itimerval newt;
	struct timeval t;

	t.tv_sec = floor(secs);
	t.tv_usec = (secs - floor(secs)) * pow(10, 6);
	newt.it_interval = t;
	newt.it_value = t;


	//printf("%ld, %ld\n", t.tv_sec, t.tv_usec);
	setitimer(ITIMER_VIRTUAL, &newt, NULL);
}

int main(int argc, char *argv[])
{
	struct rc4_ctx ctx;
	unsigned char *buf = NULL;

	initialize_options(argc, argv);

	rc4_init_key(&ctx, "Ks#gh(a@jks!01GJ;b", 16);

	printf("Total ram: %ld\n", (total_ram));
	if(total_time > 0.0)	{
		set_timer(total_time);
	}

	//return 0;
	buf = malloc(total_ram);

	setup_signals();
	if(buf == NULL)	{
		fprintf(stderr, "Error allocating %ld bytes of RAM\n", total_ram);
		return EXIT_FAILURE;
	}

	while(1)	{
		rc4_xor_stream(&ctx, buf, total_ram);
	}

	free(buf);
	return EXIT_SUCCESS;
}

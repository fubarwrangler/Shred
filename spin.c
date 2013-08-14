/****************************************************************************
 * spin.c - a memory-consuming program that spins the CPU as well
 *
 *	Designed to test Condor batch-system policy regarding memory usage and
 *	CPU-time usage.  Allocates the requested amount of memory then churns
 *	through it over and over until a timer runs out or a SIGINT/TERM kills
 *	the program.
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
#include "cmdlineparse.h"

double total_time = -1.0;
size_t total_ram = (1 << 24);
bool keep_going = true;
int chunks = 0;

int stop_count = 0;

/* If sigint, set done=1 and break out of main loop cleanly */
static void sigint_handler(int signum)
{
	if(signum == SIGVTALRM)	{
		fprintf(stderr, "Timer expired, exit now\n");
		exit(0);
	} else if(signum == SIGINT || signum == SIGTERM) {
		if(stop_count > 0)
			exit(1);
		keep_going = false;
		fprintf(stderr, "Signal caught, terminating\n");
		stop_count++;
	}
}

/* Setup signal handler */
static void setup_signals(void)
{
	struct sigaction new_action;
	sigset_t self;

	sigemptyset(&self);
	sigaddset(&self, SIGVTALRM);
	sigaddset(&self, SIGINT);
	sigaddset(&self, SIGTERM);
	new_action.sa_handler = sigint_handler;
	new_action.sa_mask = self;
	new_action.sa_flags = 0;

	sigaction(SIGINT, &new_action, NULL);
	sigaction(SIGTERM, &new_action, NULL);
	sigaction(SIGVTALRM, &new_action, NULL);
}


/* Set the configuration options above from cmdline */
static void initialize_options(int argc, char *argv[])
{
	int c;

	while((c=getopt(argc, argv, "+hn:t:c:")) != -1)	{
		switch(c)	{
			case 'n':
				total_ram = parse_num(c);
				break;
			case 'c':
				chunks = parse_num(c);
				break;
			case 't':
				total_time = parse_dbl(c);
				break;
			case 'h':
				fprintf(stderr,
"Usage: %s [OPTION] [DESTINATION]\n\
  Options:\n\
    -n  amount of RAM to allocate (defaults to 16Mb)\n\
    -c  number of chunks to make up total RAM (ram < 2^20 : 1, else ~log(ram))\n\
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

	printf("Set timer for %.2fs\n", secs);

	setitimer(ITIMER_VIRTUAL, &newt, NULL);
}

int main(int argc, char *argv[])
{
	struct rc4_ctx ctx;
	unsigned char **buf = NULL;
	unsigned char **bufs = NULL;
	size_t each_chunk, ctr;
	int i;

	initialize_options(argc, argv);

	rc4_init_key(&ctx, (unsigned char *)"Ks#gh(a@jks!01GJ;b", 16);

	if(total_time > 0.0)	{
		set_timer(total_time);
	}
	setup_signals();

	if(chunks == 0)	{
		long double d = log(total_ram);
		chunks = 1;
		if(d > 20.0)
			chunks = d / 4.0;

	}
	printf("Total ram: %ld (%d chunks)\n", total_ram, chunks);

	bufs = calloc((chunks + 1), sizeof(unsigned char *));
	if(bufs == NULL)	{
		fprintf(stderr, "Error allocating RAM for chunks!?\n");
		return EXIT_FAILURE;
	}

	each_chunk = total_ram / chunks;

	for(i = 0; i < chunks; i++)	{
		bufs[i] = malloc(each_chunk + 1);
		memset(bufs[i], 0x7f, each_chunk + 1);
		if(bufs[i] == NULL)	{
			fprintf(stderr, "Error allocating chunk %d/%d (%ld each)\n",
							i, chunks, each_chunk);
			return EXIT_FAILURE;
		}
	}
	ctr = 0;
	while(keep_going)	{
		buf = bufs;
		while(*buf && keep_going)
			rc4_xor_stream(&ctx, *buf++, each_chunk);
		ctr++;
	}

	printf("Got through: %ld or fewer iterations of %ld bytes\n", ctr, total_ram);

	/* why not */
	for(buf = bufs; *buf; buf++)	{
		free(*buf);
	}
	free(bufs);


	return EXIT_SUCCESS;
}

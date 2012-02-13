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
#include <signal.h>

#include "rc4.h"

/* Length of the key to read from /dev/urandom each re-initialization */
static size_t klen = 32;
/* Size of the buffer to write at a time */
static size_t bufsize = 4096;
/* Number of buffers to write before re-initalization of cipher */
static size_t reps = 8192 << 3;
/* Total number of bytes to output */
static long int total = -1;
/* Filename to write to, stdout if NULL */
static char *fname = NULL;
/* Print the configuration to stderr */
static int print_conf = 0;
/* Debug messages along the way */
static int debug = 0;
/* Break out of loop */
static int done = 0;


static void sigint_handler(int signum)
{
	done = 1;
	if(debug || print_conf)
		fputs("\nCaught SIGINT, stop after next block...", stderr);
	if(print_conf && !debug)
		fputc('\n', stderr);
}

static void setup_signals(void)
{
	struct sigaction new_action;
	sigset_t self;

	sigemptyset(&self);
	sigaddset(&self, SIGINT);
	new_action.sa_handler = sigint_handler;
	new_action.sa_mask = self;
	new_action.sa_flags = 0;

	sigaction(SIGINT, &new_action, NULL);
}

/* Get a positive integer out of the current optarg (option @c) or exit */
static long int parse_num(int c)
{
	char *p;
	long int n;

	errno = 0;
	n = strtoul(optarg, &p, 10);
	if(errno || *p != '\0' || n < 0)	{
		fprintf(stderr, "Error -%c requires a positive integer\n", c);
		exit(1);
	}
	return n;
}

/* Set the configuration options above from cmdline */
void initialize_options(int argc, char *argv[])
{
	int c;

	while((c=getopt(argc, argv, "+hpdn:k:b:r:f:")) != -1)	{
		switch(c)	{
			case 'n':
				total = parse_num(c);
				break;
			case 'k':
				klen = parse_num(c);
				if(klen > 256)	{
					fputs("Warning: only 256-bytes of key will be used\n",
						  stderr);
					klen = 256;
				}

				break;
			case 'b':
				bufsize = parse_num(c);
				break;
			case 'r':
				reps = parse_num(c);
				break;
			case 'h':
				fprintf(stderr,
"Usage: %s [-kbrnh] [dest]\n\
  Options:\n\
    -n  total number of blocks to write, default unlimited\n\
    -b  block size to write at a time, default 4096\n\
    -r  blocks to write before re- initializing the key, default 8192\n\
    -k  number of random bytes to initialize the key, default 32\n\
    -p  print the configuration used to stderr\n\
    -d  debug, print processing messages to stderr (implies -p)\n\n\
  Arguments:\n\
    dest  optional output destination, defaults to stdout\n\
", argv[0]);
				exit(0);
			case 'd':
				debug = 1;
			case 'p':
				print_conf = 1;
				break;
			case '?':
				if(strchr("nkbr", optopt) == NULL)
					fprintf(stderr,
						"Unknown option -%c encountered\n", optopt);
				else
					fprintf(stderr,
						"Option -%c requires an argument\n", optopt);
				exit(1);
			default:
				abort();
		}
	}

	if(optind + 1 == argc)	{
		fname = strdup(argv[optind]);
		if(fname == NULL)	{
			fputs("Error no memory for filename\n", stderr);
			exit(1);
		}
	} else if(optind != argc) {
		fprintf(stderr, "Invalid/too many arguments found\n");
		exit(1);
	}
}

/* Read @len random bytes from /dev/urandom into @buf */
static void read_random_bytes(char *rand_device, unsigned char *buf, size_t len)
{
	int fd;
	fd = open(rand_device, O_RDONLY);
	if (fd >= 0) {
		size_t rb = 0;
		while(rb < len)	{
			rb += read(fd, buf + rb, len - rb);
		}
		close(fd);
	} else {
		perror("Random device");
		exit(1);
	}
}

static int write_block(int fd, unsigned char *buf, size_t len)
{
	size_t written = 0;
	ssize_t this_write = 0;

	while(written < len)	{

		this_write = write(fd, buf + written, len - written);

		if(errno || this_write < 0)	{
			if(errno == ENOSPC)	{
				fputs("\nNo space left, exiting\n", stderr);
				exit(0);
			}
			perror("Writing data");
			exit(1);
		}
		written += this_write;
	}
	return 1;
}


int main(int argc, char *argv[])
{
	unsigned char *data, *key;
	unsigned char tmpdata[8];
	unsigned int n;
	struct rc4_ctx ctx;
	size_t written = 0;
	int fd;

	initialize_options(argc, argv);

	data = malloc(bufsize);
	key = malloc(klen);

	if(data == NULL || key == NULL)	{
		fputs("Memory allocation error\n", stderr);
		return 1;
	}

	if(print_conf)	{
		char tstr[64];

		if(total < 0) strcpy(tstr, "(unlimited)");
		else snprintf(tstr, 63, "%ld", total);

		fprintf(stderr,
			"Block size: %d\nBlocks / key: %d\nKey bytes: %d\n"
			"Total: %s\nDestination: %s\n",
			bufsize, reps, klen, tstr,
			(fname == NULL) ? "(stdout)" : fname);

	}

	if(fname != NULL)	{
		fd = open(fname, O_CREAT | O_WRONLY, 0664);
		if(fd < 0)	{
			char warn[2048];
			snprintf(warn, 2047, "Opening '%s' for writing", fname);
			perror(warn);
			exit(1);
		}
	} else {
		fd = fileno(stdout);
	}

	if(debug) fprintf(stderr, "Initalizing key with %d bytes\n", klen);

	/* First pass init key with 64 truly random bits */
	read_random_bytes("/dev/random", tmpdata, 8);
	rc4_init_key(&ctx, tmpdata, 8);

	setup_signals();

	do {
		for(n = 0; n < reps && !done; n++)	{
			rc4_fill_buf(&ctx, data, bufsize);

			written += write_block(fd, data, bufsize);

			if(total >= 0 && written >= total)
				done = 1;
		}
		read_random_bytes("/dev/urandom", key, klen);

		/* Mix the state with more random bytes */
		rc4_shuffle_key(&ctx, key, klen);

		if(debug && !done)
			fprintf(stderr,
				"Reinitalizing key, %d blocks so far (%.3f Mb)\r",
				written, (float)((written * bufsize) / 1000000.0));
	} while(!done);

	if(debug)
		fprintf(stderr,
			"\nFinished, %d blocks (%.3f Mb) written in total\n",
			written, (float)((written * bufsize) / 1000000.0 ));

	if(fname != NULL)
		close(fd);

	free(data);
	free(key);

	return 0;
}
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
#include <time.h>
#include <fcntl.h>
#include <errno.h>
#include <stdbool.h>
#include <signal.h>
#include <sys/types.h>

#include "rc4.h"
#include "cmdlineparse.h"

/* Length of the key to read from /dev/urandom each re-initialization */
static size_t klen = 32;
/* Size of the buffer to write at a time */
static size_t bufsize = 4096;
/* Number of buffers to write before re-initalization of cipher */
static size_t reps = 8192;
/* Total number of bytes to output */
static size_t total = 0;
/* Filename to write to, stdout if NULL */
static char *fname = NULL;
/* Print the configuration to stderr */
static bool print_conf = 0;
/* Debug messages along the way */
static bool debug = false;
/* Break out of loop */
static bool done = false;
/* Bytes to skip in output device before writing */
static off_t skip = 0;


/* If sigint, set done=1 and break out of main loop cleanly */
static void sigint_handler(int signum)
{
	done = true;
	if(debug || print_conf)
		fputs("\nCaught SIGINT, stop after next block...", stderr);
	if(signum && print_conf && !debug)
		fputc('\n', stderr);
}

/* Setup signal handler */
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

/* Set the configuration options above from cmdline */
static void initialize_options(int argc, char *argv[])
{
	int c;

	while((c=getopt(argc, argv, "+hpdn:k:b:r:f:s:")) != -1)	{
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
			case 's':
				skip = parse_num(c);
				break;
			case 'h':
				fprintf(stderr,
"Usage: %s [OPTION] [DESTINATION]\n\
  Options:\n\
    -n  total number of blocks to write, default unlimited\n\
    -b  block size to write at a time, default 4096\n\
    -r  blocks to write before re- initializing the key, default 8192\n\
    -k  number of random bytes to initialize the key, default 32\n\
    -s  bytes to skip in output device before starting writing\n\
    -p  print the configuration used to stderr\n\
    -d  debug, print processing messages to stderr (implies -p)\n\n\
  Arguments:\n\
    DESTINATION  optional output destination, defaults to stdout\n\n\
  Notes:\n\
    Any numeric value can be postfixed with a multiplier, one of the\n\
    following letters:\n\
      k/K m/M g/G\n\
    for kilo, mega, or giga-byte. The lower-case versions return the power\n\
    of two nearest (1k = 1024), and the upper-case returns an exact power of\n\
    ten (1K = 1000).\n\
", argv[0]);
				exit(EXIT_SUCCESS);
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
				exit(EXIT_FAILURE);
			default:
				abort();
		}
	}

	if(optind + 1 == argc)	{
		fname = argv[optind];
	} else if(optind != argc) {
		fprintf(stderr, "Invalid/too many arguments found\n");
		exit(EXIT_FAILURE);
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
		exit(EXIT_FAILURE);
	}
}

/* Write @len bytes from @buf out to file descriptor @fd.
 * Return 1 on success, 0 on full device, exit on failure
 */
static int write_block(int fd, unsigned char *buf, size_t len)
{
	size_t written = 0;
	ssize_t this_write = 0;

	while(written < len)	{

		this_write = write(fd, buf + written, len - written);

		if(errno || this_write < 0)	{
			if(errno == ENOSPC)	{
				fputs("No space left, exiting", stderr);
				done = true;
				return 0;
			}
			perror("Writing data");
			exit(EXIT_FAILURE);
		}

		written += this_write;
	}
	return 1;
}


int main(int argc, char *argv[])
{
	unsigned char *data, *key;
	unsigned char tmpdata[8];
	unsigned char discard[1024];
	unsigned int n;
	struct rc4_ctx ctx;
	size_t written = 0;
	struct timespec t_start, t_end;
	float mb, runtime;
	int fd;

	initialize_options(argc, argv);

	data = malloc(bufsize);
	key = malloc(klen);

	if(data == NULL || key == NULL)	{
		fputs("Memory allocation error\n", stderr);
		return EXIT_FAILURE;
	}

	if(print_conf)	{
		char tstr[64];

		if(total == 0) strcpy(tstr, "(unlimited)");
		else snprintf(tstr, 63, "%ld", total);

		fprintf(stderr,
			"Block size: %ld\nBlocks / key: %ld\nKey bytes: %ld\n"
			"Total: %s\nDestination: %s (%ld bytes skipped)\n",
			bufsize, reps, klen, tstr,
			(fname == NULL) ? "(stdout)" : fname, skip);

	}

	if(fname != NULL)	{
		fd = open(fname, O_CREAT | O_WRONLY, 0664);
		if(fd < 0)	{
			char warn[2048];
			snprintf(warn, 2047, "Opening '%s' for writing", fname);
			perror(warn);
			exit(EXIT_FAILURE);
		}
		if(skip > 0)	{
			if(lseek(fd, skip, SEEK_SET) < 0)	{
				perror("Failed to seek in output");
				exit(EXIT_FAILURE);
			}
			if(debug) fprintf(stderr, "Seeked %ld bytes in output\n", skip);
		}
	} else {
		fd = fileno(stdout);
	}

	if(debug) fprintf(stderr, "Initalizing key with %ld bytes\n", klen);

	/* First pass init key with 96 truly random bits */
	read_random_bytes("/dev/random", tmpdata, 12);
	rc4_init_key(&ctx, tmpdata, 12);

	setup_signals();

	if(clock_gettime(CLOCK_MONOTONIC, &t_start) != 0)	{
		perror("clock_gettime");
		return 1;
	}

	do {
		read_random_bytes("/dev/urandom", key, klen);

		/* Mix the state with more random bytes */
		rc4_shuffle_key(&ctx, key, klen);
		
		/* Discard some keystream because beginning of RC4 is weaker?
		 * I mean, we're not really doing crypto here, but whatever...
		 */
		rc4_fill_buf(&ctx, discard, sizeof(discard));
		

		for(n = 0; n < reps && !done; n++)	{
			rc4_fill_buf(&ctx, data, bufsize);

			written += write_block(fd, data, bufsize);

			if(total > 0 && written >= total)
				done = true;
		}

		if(debug && !done)
			fprintf(stderr,
				"Reinitalizing key, %ld blocks so far (%.3f Mb)\r",
				written, (float)((written * bufsize) / 1000000.0));
	} while(!done);

	free(data);
	free(key);

	if(fsync(fd) < 0)	{
		if(errno == EIO || errno == EBADF)	{
			perror("Final sync");
			return EXIT_FAILURE;
		}
	}

	if(clock_gettime(CLOCK_MONOTONIC, &t_end) != 0)	{
		perror("clock_gettime");
		return 1;
	}

	mb = (float)((written * bufsize) / 1000000.0f);
	runtime = (t_end.tv_sec - t_start.tv_sec) +
			  ((float)(t_end.tv_nsec - t_start.tv_nsec) / 1000000000.0f);

	fprintf(stderr, "\nFinished, %ld blocks (%.3f Mb) written in %.3fs (%.2f Mb/s)\n",
					written, mb, runtime, mb / runtime);

	if(fname != NULL)
		close(fd);

	return EXIT_SUCCESS;
}

/* vim: set ts=4 sw=4 noexpandtab: */
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
//#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <errno.h>
#include <stdbool.h>
#include <signal.h>
#include <pthread.h>
#include <assert.h>
#include <sys/types.h>

#include "rc4.h"
#include "cmdlineparse.h"

void read_random_bytes(const char *rand_device, unsigned char *buf, size_t len);
int write_block(int fd, unsigned char *buf, size_t len);


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
/* Open with O_DIRECT */
static bool direct_io = false;
/* Debug messages along the way */
static bool debug = false;
/* Break out of loop */
static bool done = false;
/* Bytes to skip in output device before writing */
static off_t skip = 0;
/* Number of threads to create, defaults to none (1 == main thread) */
static int nr_threads = 1;

static struct per_thread	{
	pthread_cond_t go;
	pthread_mutex_t lock;
	struct rc4_ctx *ctx;
	bool ready;
	unsigned char *buf;
	int id;
} *tinfo;

static pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t dataready = PTHREAD_COND_INITIALIZER;


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

	while((c=getopt(argc, argv, "+hpdSn:k:b:r:f:s:t:")) != -1)	{
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
			case 'S':
				direct_io = true;
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
			case 't':
				nr_threads = parse_num(c);
				break;
			case 'h':
				fprintf(stderr,
"Usage: %s [OPTION] [DESTINATION]\n\
  Options:\n\
    -n  total number of blocks to write, default unlimited\n\
    -b  block size to write at a time, default 4096\n\
    -r  blocks to write before re- initializing the key, default 8192\n\
    -k  number of random bytes to initialize the key, default 32\n\
    -S  sidestep disk buffer, open destination with O_DIRECT\n\
    -s  bytes to skip in output device before starting writing\n\
    -t  number of threads to use, default is just main thread\n\
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
				debug = true;
			case 'p':
				print_conf = true;
				break;
			case '?':
				if(strchr("nkbrs", optopt) == NULL)
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

static void init_threads(struct rc4_ctx *root)
{
	unsigned char key[16];

	for (int i = 0; i < nr_threads; i++)	{
		printf("Initalizing thread %d\n", i);
		pthread_mutex_init(&(tinfo[i].lock), NULL);
		pthread_cond_init(&(tinfo[i].go), NULL);
		tinfo[i].ctx = rc4_copy_ctx(root);

		/* Mix the state with more random bytes */
		read_random_bytes("/dev/urandom", key, sizeof(key));
		rc4_shuffle_key(tinfo[i].ctx, key, sizeof(key));

		tinfo[i].ready = false;
		tinfo[i].buf = malloc(bufsize);
		tinfo[i].id = i;
	}
}

static void *worker_generator(void *arg)
{
	int id = (int)arg;
	printf("Creating prod worker %d\n", id);
	assert(id < nr_threads);

	struct per_thread *pt = &tinfo[id];

	pthread_mutex_lock(&pt->lock); /* L */
	printf("prod-%d: Entering worker, locked mtx\n", id);
	while(!done)	{
		// printf("prod-%d: Fill my buf (%p)\n", id, pt->buf);
		rc4_fill_buf(pt->ctx, pt->buf, bufsize);
		pt->ready = true;
		// printf("prod-%d: Buf full, waiting for global mtx to signal ready\n", id);
		pthread_mutex_lock(&mtx);
		pthread_cond_signal(&dataready);
		pthread_mutex_unlock(&mtx);
		// printf("prod-%d: Ready signalled -- wait for my cond\n", id);
		while(pt->ready)	{
			pthread_cond_wait(&pt->go, &pt->lock); /* U ... L */
		}
	}
	pthread_mutex_unlock(&pt->lock); /* U */
	return NULL;
}

static unsigned char *get_available_data(void)
{
	static int last_id = 0;
	struct per_thread *t;
	int i = last_id;

	// printf("Main: Enter get_avail: last = i = %d/%d\n", last_id, i);

	t = &tinfo[last_id];
	pthread_mutex_lock(&t->lock);
	t->ready = false;
	pthread_mutex_unlock(&t->lock);
	pthread_cond_signal(&t->go);
	pthread_mutex_lock(&mtx);
	while(true)	{

		/* Checks if any are ready */
		for(int n = 0; n < nr_threads; n++)	{
			i = (i + 1) % nr_threads;
			t = &tinfo[i];
			if(t->ready)	{
				last_id = i;
				pthread_mutex_unlock(&mtx);
				// printf("Main: Got data - %d: unlock main & return\n", i);
				return t->buf;
			} else {
				// printf("Main: %d not ready\n", i);
			}
		}
		// printf("Main: no data ready -- wait for dataready\n");
		pthread_cond_wait(&dataready, &mtx);
	}
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

	pthread_t producers[nr_threads];

	if (nr_threads > 200) {
		fputs("Too many threads, must be <= 200\n", stderr);
		return EXIT_FAILURE;
	}

	data = malloc(bufsize);
	key = malloc(klen);
	tinfo = calloc(nr_threads, sizeof(struct per_thread));

	if(data == NULL || key == NULL || tinfo == NULL)	{
		fputs("Memory allocation error\n", stderr);
		return EXIT_FAILURE;
	}

	if(print_conf)	{
		char tstr[64];

		if(total == 0) strcpy(tstr, "(unlimited)");
		else snprintf(tstr, 63, "%ld", total);

		fprintf(stderr,
			"Block size: %ld\nBlocks / key: %ld\nKey bytes: %ld\n"
			"Total: %s\nDestination: %s (%ld bytes skipped)%s",
			bufsize, reps, klen, tstr,
			(fname == NULL) ? "(stdout)" : fname, skip,
			(direct_io) ? "\nDirect IO (O_DSYNC) in use\n" : "\n");

	}

	if(fname != NULL)	{
		int flags = O_CREAT | O_WRONLY;
		fd = open(fname, flags | ((direct_io) ? O_DSYNC : 0), 0664);
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
	read_random_bytes("/dev/random", tmpdata, sizeof(tmpdata));
	rc4_init_key(&ctx, tmpdata, sizeof(tmpdata));

	setup_signals();

	if(clock_gettime(CLOCK_MONOTONIC, &t_start) != 0)	{
		perror("clock_gettime");
		return 1;
	}

	/* Discard some keystream because beginning of RC4 is weaker?
	 * I mean, we're not really doing crypto here, but whatever...
	 */
	rc4_fill_buf(&ctx, discard, sizeof(discard));

	if(nr_threads > 1)	{
		init_threads(&ctx);
		for(int i = 0; i < nr_threads; i++)	{
			pthread_create(&producers[i], NULL, worker_generator, (void *)i);
		}
	}


	do {
		read_random_bytes("/dev/urandom", key, klen);

		/* Mix the state with more random bytes */
		rc4_shuffle_key(&ctx, key, klen);



		for(n = 0; n < reps && !done; n++)	{
			unsigned char *d;
			if(nr_threads > 1)	{
				d = get_available_data();
			} else {
				rc4_fill_buf(&ctx, data, bufsize);
				d = data;
			}

			if(write_block(fd, d, bufsize) == 0)	{
				done = true;
			} else {
				written++;
			}

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

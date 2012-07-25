#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <getopt.h>

#include <stdbool.h>

#include "rc4.h"


static char   passphrase[256] = {0};
static size_t passlen = 0;
static bool   have_pass = false;
static char  *input_file = NULL;
static char  *output_file = NULL;


/* Stupid, but apparently getpass() is deprecated */
static void read_password_terminal(char *prompt)
{
	struct termios oldterm, newterm;
	ssize_t n = 0;
	ssize_t plen = strlen(prompt);
	int i = 0;
	int fd;
	char c;

	if(!isatty(fileno(stdin)))	{
		if((fd = open("/dev/tty", O_RDWR)) < 0)	{
			perror("Open terminal");
			exit(EXIT_FAILURE);
		}
	} else {
		fd = fileno(stdin);
	}

	do {
		n += write(fd, prompt, plen - n);
	} while(n < plen);

	tcgetattr(fd, &oldterm);
	newterm = oldterm;

	/*setting the approriate bit in the termios struct*/
	newterm.c_lflag &= ~(ECHO);
	newterm.c_lflag |= ECHONL;


	/*setting the new bits*/
	tcsetattr(fd, TCSANOW, &newterm);

	while (i < 255)	{

		n = read(fd, &c, 1);
		if(n < 1)	{
			perror("Reading from terminal");
			exit(EXIT_FAILURE);
		}
		if(c == '\n')
			break;

		passphrase[i++] = c;
	}
	passphrase[i] = '\0';

	/* resetting our old terminal settings */
	tcsetattr(fd, TCSANOW, &oldterm);

	if(fd != fileno(stdin))
		close(fd);
}

static void read_passfile(const char *filename)
{
	FILE *fp = fopen(filename, "r");

	if(fp == NULL)	{
		fprintf(stderr, "Error, cannot open pass-file %s\n", filename);
		exit(EXIT_FAILURE);
	}

	passlen = fread(passphrase, 1, 256, fp);

	if(passlen == 0)	{
		fputs("WARNING: empty passphrase encountered\n", stderr);
	} else if(ferror(fp))	{
		fputs("ERROR: file-error reading from pass-file\n", stderr);
		exit(EXIT_FAILURE);
	} else if (!feof(fp))	{
		fputs("WARNING: only the first 256 bytes of pass-file are used\n", stderr);
	}

	fclose(fp);
}

/* Set the configuration options above from cmdline */
static void initialize_options(int argc, char *argv[])
{
	int c;

	/* Use getopt_long here because with POSIX feature-tets-macro set we don't
	 * permute option strings, but we want to because we're lazy
	 */
	while((c=getopt_long(argc, argv, "hp:f:", NULL, NULL)) != -1)	{
		switch(c)	{
			case 'p':
				if(optarg == NULL)	{
					fputs("Error, no password specified in -p option\n", stderr);
					exit(EXIT_FAILURE);
				}
				passlen = strlen(optarg) < 255 ? strlen(optarg) : 255;
				strncpy(&passphrase[0], optarg, 255);
				have_pass = true;
				break;
			case 'f':
				read_passfile(optarg);
				have_pass = true;
				break;
			case 'h':
				fprintf(stderr,
"Usage: %s [OPTION] [INPUT] [OUTPUT]\n\
  Options:\n\
    -p  passphrase to use, if not given prompt from user on stdin\n\
    -f  pass-file to use, read contents of file and user as passphrase\n\n\
  Arguments:\n\
    INPUT   optional input file, if not given or given as '-', read stdin\n\
    OUTPUT  optional output file, if not given write to stdout\n\n\
  Notes:\n\
    If reading from stdin, the user will be asked to provide a password\n\
    from the terminal, so unless -p or -f is specified, the program needs \n\
    a controlling terminal or it will throw an error.\n\
", argv[0]);
				exit(EXIT_SUCCESS);
			case '?':
				exit(EXIT_FAILURE);
			default:
				abort();
		}
	}

	if(argc > optind)
		input_file = !strcmp(argv[optind], "-") ? NULL : argv[optind];
	if(argc > optind + 1)
		output_file = !strcmp(argv[optind + 1], "-") ? NULL : argv[optind + 1];
	if(argc > optind + 2)	{
		fputs("ERROR: too many arguments specified\n", stderr);
		exit(EXIT_FAILURE);
	}
}

#define BUF_SIZE 1024

int main(int argc, char *argv[])
{
	FILE *fp_in, *fp_out;
	unsigned char buf[BUF_SIZE];
	struct rc4_ctx ctx;
	size_t nread;

	initialize_options(argc, argv);

	read_password_terminal("Password: ");

	rc4_init_key(&ctx, passphrase, passlen);

	if(input_file == NULL)	{
		fp_in = stdin;
	} else if((fp_in = fopen(input_file, "r")) == NULL)	{
		fprintf(stderr, "ERROR: cannot open inupt file '%s'\n", input_file);
		return 1;
	}

	if(output_file == NULL)	{
		fp_out = stdout;
	} else if((fp_out = fopen(output_file, "w")) == NULL)	{
		fprintf(stderr, "ERROR: cannot open output file '%s'\n", output_file);
		return 1;
	}

	while((nread = fread(buf, 1, BUF_SIZE, fp_in)) != 0)	{

		rc4_xor_stream(&ctx, buf, nread);

		if(fwrite(buf, 1, nread, fp_out) != nread)	{
			fprintf(stderr, "ERROR: write-error to output file\n");
			return 1;
		}

	}

	fclose(fp_in);
	fclose(fp_out);


	return 0;
}

#include <stdio.h>
#include <stdint.h>


uint64_t arr[256] = {0};

int main(int argc, char *argv[])
{
	FILE *fp = stdin;
	unsigned char buf[4096];
	uint64_t total = 0;
	uint64_t norm = 0;
	uint64_t max = 0;
	size_t n, i;

	if(argc > 1)	{
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

	for(int j = 0; j < 256; j++)	{
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

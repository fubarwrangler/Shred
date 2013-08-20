#include <stdio.h>
#include <stdint.h>


uint64_t arr[256] = {0};

int main(int argc, char *argv[])
{
	int r;
	uint64_t max = 0, norm = 0;
	unsigned char c;
	
	while((r = getchar()) != EOF)	{
		c = (unsigned char)r;
		arr[c]++;
	}

	for(int i = 0; i < 256; i++)	{
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


	return 0;
}

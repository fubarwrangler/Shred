CFLAGS= -O2 -march=core2 -Wall

shred: rc4.o shred.o
	$(CC) $(CFLAGS) -o shred $^

%.o: %.c
	$(CC) $(CFLAGS) -c $^

test: rc4.c
	$(CC) $(CFLAGS) -D TEST -o rc4-test rc4.c

clean:
	rm -f *.o rc4-test rc4 shred

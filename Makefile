CFLAGS= -O3 -march=core2 -Wall -pedantic -Wextra -std=c99 -D_POSIX_C_SOURCE=2
PREFIX= /usr

shred: rc4.o shred.o
	$(CC) -o shred $^

%.o: %.c
	$(CC) $(CFLAGS) -c $^

test: rc4.c
	$(CC) $(CFLAGS) -D TEST -o rc4-test rc4.c

install: shred
	chmod 755 shred
	chown root.root shred
	mv shred $(PREFIX)/bin/dshred

clean:
	rm -f *.o rc4-test rc4 shred

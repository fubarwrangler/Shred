# CFLAGS= -O3 -march=native -Wall -pedantic -Wextra -std=c99 -D_POSIX_C_SOURCE=200112L
CFLAGS= -Wall -g -pedantic -Wextra -std=c99 -D_POSIX_C_SOURCE=200112L
#LDFLAGS=-static
LDFLAGS=
PREFIX= /usr

all: shred rc4filter spin stride dist

dist: dist.o cmdlineparse.o
	$(CC) $(LDFLAGS) -o dist $^

shred: rc4.o shred.o shredutil.o cmdlineparse.o
	$(CC) $(LDFLAGS) -lrt -pthread -o shred $^

rc4filter: rc4.o rc4filter.o cmdlineparse.o
	$(CC) $(LDFLAGS) -o rc4filter $^

spin: rc4.o spin.o cmdlineparse.o
	$(CC) $(LDFLAGS) -o spin $^ -lm

stride: stride.o cmdlineparse.o
	$(CC) $(LDFLAGS) -o stride $^

%.o: %.c
	$(CC) $(CFLAGS) -c $^

test: rc4.c
	$(CC) $(LDFLAGS) $(CFLAGS) -D TEST -o rc4-test rc4.c

install-shred: shred
	chmod 755 shred
	chown root.root shred
	mv shred $(PREFIX)/bin/dshred

install-filter: rc4filter
	chmod 755 rc4filter
	chown root.root rc4filter
	mv rc4filter $(PREFIX)/bin/rc4filter

install-spin: spin
	chmod 755 spin
	chown root.root spin
	mv spin $(PREFIX)/bin/spin

clean:
	rm -f *.o rc4-test rc4 shred rc4filter spin stride dist

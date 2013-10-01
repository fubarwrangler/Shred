#!/usr/bin/python

import random, sys

for x in xrange(600000):
    n = int(random.gauss(128, 20))
    if n > 0 and n < 256:
        sys.stdout.write(chr(n))

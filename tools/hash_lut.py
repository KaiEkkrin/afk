#!/usr/bin/python3

# This utility generates the bytewise lookup table for AFK_Hash.

import os
import random

numbers = list(range(0, 256))
random.seed(os.urandom(8))
random.shuffle(numbers)

for n in numbers:
    print("\t{0},".format(n))


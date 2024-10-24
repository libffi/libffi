#!/usr/bin/env python3

import sys

with open(sys.argv[1], 'r') as f:
    for line in f.readlines():
        line = line.strip()
        if line and not line.startswith('#'):
            print(line)
            sys.exit(0)

sys.exit(1)

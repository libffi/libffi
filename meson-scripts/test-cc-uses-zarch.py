#!/usr/bin/env python3

import os
import sys
import subprocess

# Put output files in the builddir
os.chdir(os.environ['MESON_BUILD_ROOT'])

infile = 'zarch-conftest.c'
outfile = 'zarch-conftest.S'

with open(infile, 'w') as f:
    f.write('void foo(void) { bar(); bar(); }')

args = sys.argv[1:]
args += ['-S', infile, '-o', outfile]
res = subprocess.run(args, stdout=subprocess.PIPE, universal_newlines=True, check=True)

with open(outfile, 'r') as f:
    if 'brasl' in f.read():
        sys.exit(0)

sys.exit(1)

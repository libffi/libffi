#!/usr/bin/env python3

import os
import sys
import re
import subprocess

# Put output files in the builddir
os.chdir(os.environ['MESON_BUILD_ROOT'])

infile = 'ro-eh-frame-conftest.c'
outfile = 'ro-eh-frame-conftest.o'

with open(infile, 'w') as f:
    f.write('extern void foo (void); void bar (void) { foo (); foo (); }')

args = sys.argv[1:]
args += ['-c', infile, '-o', outfile]
res = subprocess.run(args, stdout=subprocess.PIPE, universal_newlines=True, check=True)

stdout = subprocess.check_output(['readelf', '-WS', outfile], stdout=subprocess.PIPE, universal_newlines=True)
if re.search('eh_frame .* WA', stdout):
    sys.exit(0)

sys.exit(1)

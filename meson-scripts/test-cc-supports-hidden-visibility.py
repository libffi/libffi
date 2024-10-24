#!/usr/bin/env python3

import os
import re
import sys
import subprocess

# Put output files in the builddir
os.chdir(os.environ['MESON_BUILD_ROOT'])

infile = 'visibility-conftest.c'
outfile = 'visibility-conftest.S'

with open(infile, 'w') as f:
    f.write('int __attribute__ ((visibility ("hidden"))) foo (void) { return 1; }')

args = sys.argv[1:]
args += ['-Werror', '-S', infile, '-o', outfile]
res = subprocess.run(args, stdout=subprocess.PIPE, universal_newlines=True, check=True)

with open(outfile, 'r') as f:
    if re.search('(\.hidden|\.private_extern).*foo', f.read()):
        sys.exit(0)
print('.hidden not found in the outputted assembly')
sys.exit(1)

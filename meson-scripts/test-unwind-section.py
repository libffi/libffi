#!/usr/bin/env python3

import os
import sys
import re
import subprocess

# Put output files in the builddir
os.chdir(os.environ['MESON_BUILD_ROOT'])

infile1 = 'unwind-section-conftest.s'
outfile1 = 'unwind-section-conftest.o'
infile2 = 'unwind-section-conftest.c'

with open(infile1, 'w') as f:
    f.write('''
        .text
        .globl foo
        foo:
        jmp bar
        .section .eh_frame,"a",@unwind
        bar:
    ''')

with open(infile2, 'w') as f:
    f.write('''
        extern void foo();
        int main(){foo();}
    ''')

args = sys.argv[1:]
subprocess.check_call(args + ['-Wa,--fatal-warnings', '-c', infile1, '-o', outfile1],
                      stdout=subprocess.PIPE, universal_newlines=True)
subprocess.check_call(args + [infile2, outfile1], stdout=subprocess.PIPE, universal_newlines=True)

sys.exit(0)

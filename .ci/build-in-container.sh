#!/bin/bash
set -x

export QEMU_LD_PREFIX=/usr/${HOST}
export DEJAGNU=/opt/.ci/site.exp
cd /opt
./configure ${HOST+--host=$HOST --disable-shared}
make
# `make dist` builds the PDF manual (needs texi2pdf/TeX, absent in the emulated
# containers) and is irrelevant to testing, so don't fail the build on it.
make dist || true
BOARDSDIR=/opt/.ci make check RUNTESTFLAGS="-a $RUNTESTFLAGS" || true

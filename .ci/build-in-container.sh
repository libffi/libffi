#!/bin/bash
set -x

export QEMU_LD_PREFIX=/usr/${HOST}

DEJAGNU=$(pwd)/.ci/site.exp
./configure ${HOST+--host=$HOST --disable-shared}
make
make dist
make check RUNTESTFLAGS="-a $RUNTESTFLAGS" || true

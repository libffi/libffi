#!/bin/bash
set -x

export QEMU_LD_PREFIX=/usr/${HOST}

export DEJAGNU=$(pwd)/.ci/site.exp
echo $DEJAGNU
ls -l $DEJAGNU
pwd
find .
./configure ${HOST+--host=$HOST --disable-shared}
make
make dist
BOARDSDIR=$(pwd)/.ci make check RUNTESTFLAGS="-a $RUNTESTFLAGS" || true

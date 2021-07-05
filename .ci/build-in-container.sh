#!/bin/bash
set -x

export QEMU_LD_PREFIX=/usr/${HOST}
export DEJAGNU=/opt/.ci/site.exp
echo $DEJAGNU
ls -l $DEJAGNU
cd /opt
./configure ${HOST+--host=$HOST --disable-shared}
make
make dist
BOARDSDIR=/opt/.ci make check RUNTESTFLAGS="-a $RUNTESTFLAGS" || true

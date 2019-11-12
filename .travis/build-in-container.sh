#!/bin/bash

cd /opt

export QEMU_LD_PREFIX=/usr/${HOST}

./configure ${HOST+--host=$HOST --disable-shared}
make
make dist
make check RUNTESTFLAGS="-a $RUNTESTFLAGS"

./rlgl l https://rl.gl
ID=$(./rlgl start)
./rlgl e --id=$ID --policy=https://github.com/libffi/rlgl-policy.git */testsuite/libffi.log
exit $?


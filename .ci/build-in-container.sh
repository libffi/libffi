#!/bin/bash
set -x

cd /opt

export QEMU_LD_PREFIX=/usr/${HOST}

pwd
env
ls -la

./configure ${HOST+--host=$HOST --disable-shared}
make
make dist
make check RUNTESTFLAGS="-a $RUNTESTFLAGS" || true

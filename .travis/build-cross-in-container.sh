#!/bin/bash

cd /opt

set -x
echo $PATH
export PATH=/usr/local/bin:$PATH
echo $PATH

ls -l /usr/local/bin

./configure --host=${HOST}
make
make dist
make check RUNTESTFLAGS="-a $RUNTESTFLAGS" || true



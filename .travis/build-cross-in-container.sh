#!/bin/bash

cd /opt

./configure --host=${HOST}
make
make dist
make check RUNTESTFLAGS="-a $RUNTESTFLAGS" || true



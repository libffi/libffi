#!/bin/bash

cd /opt

export QEMU_LD_PREFIX=/usr/${HOST}

if ! test "x$HOST" == "x"; then
    ./configure --host=${HOST} --disable-shared;
else
    ./configure
fi
make
make dist
make check RUNTESTFLAGS="-a $RUNTESTFLAGS"
EXITCODE=$?
gzip -c -9 */testsuite/libffi.log > libffi.log.gz
echo ================================================================
echo The logs are too long for travis to handle, so we compress and
echo uuencode them.  Download, decode and uncompress if you need to
echo read them.
echo ================================================================
uuencode libffi.log.gz -
echo ================================================================
echo ================================================================
exit $EXITCODE


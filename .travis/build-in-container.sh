#!/bin/bash

set -x

echo ===============================================================
env
echo ===============================================================

cd /opt

export QEMU_LD_PREFIX=/usr/${HOST}

./configure ${HOST+--host=$HOST --disable-shared}
make
make dist
make check RUNTESTFLAGS="-a $RUNTESTFLAGS"

gzip -c -9 */testsuite/libffi.log > libffi.log.gz
echo ================================================================
echo The logs are too long for travis to handle, so we compress and
echo uuencode them.  Download, decode and uncompress if you need to
echo read them.
echo ================================================================
uuencode libffi.log.gz -
echo ================================================================
echo ================================================================

ID=$(./rlgl start)
./rlgl e --id=$ID --policy=https://github.com/libffi/rlgl-policy.git */testsuite/libffi.log
exit $?


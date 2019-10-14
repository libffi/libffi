#!/bin/bash

# exit this script if any commmand fails
# set -e

cd /opt

env

apt-get update
apt-get install -y gcc dejagnu make texinfo sharutils gzip git
./configure 
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

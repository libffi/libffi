#!/bin/bash

# exit this script if any commmand fails
# set -e

function build_linux()
{
    ./autogen.sh
    ./configure ${HOST+--host=$HOST} ${CONFIGURE_OPTIONS}
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
}

function build_foreign_linux()
{
    docker run --rm -t -i -v /usr/bin/qemu-arm-static:/usr/bin/qemu-arm-static -v `pwd`:/opt --rm -ti arm32v7/debian:stretch-slim /opt/.travis/build-in-container.sh
}

function build_ios()
{
    which python
# export PYTHON_BIN=/usr/local/bin/python
    ./generate-darwin-source-and-headers.py
    xcodebuild -showsdks
    xcodebuild -project libffi.xcodeproj -target "libffi-iOS" -configuration Release -sdk iphoneos10.3
    find ./ 
}

./autogen.sh
case "$HOST" in
    arm-apple-darwin*)
	build_ios
	;;
    arm32v7-linux-gnu)
        build_foreign_linux
	;;
    *)
	build_linux
	;;
esac

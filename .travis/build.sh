#!/bin/bash

# exit this script if any commmand fails
# set -e
set -x

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
    docker run --rm -t -i -v /usr/bin/qemu-$1-static:/usr/bin/qemu-$1-static -v `pwd`:/opt --rm -ti -e LIBFFI_TEST_OPTIMIZATION="${LIBFFI_TEST_OPTIMIZATION}" $2 /opt/.travis/build-in-container.sh
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
        build_foreign_linux arm moxielogic/arm32-ci-build-container:latest 
	;;
    aarch64-linux-gnu)
        build_foreign_linux aarch64 moxielogic/aarch64-ci-build-container:latest 
	;;
    s390x-linux-gnu)
	build_foreign_linux s390x s390x/ubuntu
	;;
    *)
	build_linux
	;;
esac

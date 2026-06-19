#!/bin/bash

set -x

# Special build tools are here...
export PATH=$HOME/i/bin:$PATH

# rlgl is now a self-contained client-side tool: it evaluates the test log
# against the git-hosted policy locally (no server login required).

if [ -z ${QEMU_CPU+x} ]; then
    export SET_QEMU_CPU=
else
    export SET_QEMU_CPU="-e QEMU_CPU=${QEMU_CPU}"
fi

export DOCKER=docker

if ! command -v makeinfo >/dev/null 2>&1; then
    CONFIGURE_OPTIONS="--disable-docs ${CONFIGURE_OPTIONS}"
fi

function build_linux()
{
    ./configure ${HOST+--host=$HOST} ${CONFIGURE_OPTIONS} || cat */config.log
    ls -l */config.log
    cat */config.log
    make
    make dist
    DEJAGNU=$(pwd)/.ci/site.exp BOARDSDIR=$(pwd)/.ci runtest --version
    DEJAGNU=$(pwd)/.ci/site.exp BOARDSDIR=$(pwd)/.ci make check RUNTESTFLAGS="-a $RUNTESTFLAGS"

    ./rlgl e -l project=libffi -l sha=${GITHUB_SHA:0:7} -l CC='${CC}' ${HOST+-l host=$HOST} --policy=https://github.com/libffi/rlgl-policy.git */testsuite/libffi.log
    exit $?
}

function build_foreign_linux()
{
    ${DOCKER} run --rm -t -v $(pwd):/opt ${SET_QEMU_CPU} -e LIBFFI_TEST_OPTIMIZATION="${LIBFFI_TEST_OPTIMIZATION}" $2 bash -c /opt/.ci/build-in-container.sh

    ./rlgl e -l project=libffi -l sha=${GITHUB_SHA:0:7} -l CC="$CC" ${HOST+-l host=$HOST} --policy=https://github.com/libffi/rlgl-policy.git */testsuite/libffi.log
    exit $?
}

function build_cross_linux()
{
    ${DOCKER} run --rm -t -v $(pwd):/opt ${SET_QEMU_CPU} -e HOST="${HOST}" -e CC="${HOST}-gcc-8 ${GCC_OPTIONS}" -e CXX="${HOST}-g++-8 ${GCC_OPTIONS}" -e LIBFFI_TEST_OPTIMIZATION="${LIBFFI_TEST_OPTIMIZATION}" quay.io/moxielogic/cross-ci-build-container:latest bash -c /opt/.ci/build-in-container.sh

    ./rlgl e -l project=libffi -l sha=${GITHUB_SHA:0:7} -l CC="${HOST}-gcc-8 ${GCC_OPTIONS}" -l host=${HOST} --policy=https://github.com/libffi/rlgl-policy.git */testsuite/libffi.log
    exit $?
}

function build_cross()
{
    ${DOCKER} pull quay.io/moxielogic/libffi-ci-${HOST}
    ${DOCKER} run --rm -t -v $(pwd):/opt -e HOST="${HOST}" -e CC="${HOST}-gcc ${GCC_OPTIONS}" -e CXX="${HOST}-g++ ${GCC_OPTIONS}" -e RUNNER_WORKSPACE=/opt -e RUNTESTFLAGS="-vv ${RUNTESTFLAGS}" -e LIBFFI_TEST_OPTIMIZATION="${LIBFFI_TEST_OPTIMIZATION}" quay.io/moxielogic/libffi-ci-${HOST} bash -c /opt/.ci/build-cross-in-container.sh

    ./rlgl e -l project=libffi -l sha=${GITHUB_SHA:0:7} -l CC="${HOST}-gcc" -l host=$HOST --policy=https://github.com/libffi/rlgl-policy.git */testsuite/libffi.log
    exit $?
}

# Cross-compile on the host with a Debian cross toolchain and run the tests
# under qemu-user (binfmt).  Used for big-endian PowerPC variants, which have
# no ready foreign-arch container image.  Expects ${HOST}-gcc / ${HOST}-g++ and
# qemu-user-static to be installed, plus the target sysroot at /usr/${HOST}.
function build_cross_qemu()
{
    # Export the sysroot so qemu-user (invoked via binfmt when the test
    # binaries run) finds the target dynamic loader; otherwise every execution
    # test fails with "Could not open '/lib/ld.so.1'".
    export QEMU_LD_PREFIX=/usr/${HOST}
    ./configure --host=$HOST CC="${HOST}-gcc ${GCC_OPTIONS}" CXX="${HOST}-g++ ${GCC_OPTIONS}" --disable-shared || cat */config.log
    make
    DEJAGNU=$(pwd)/.ci/site.exp BOARDSDIR=$(pwd)/.ci \
        make check RUNTESTFLAGS="-a $RUNTESTFLAGS" || true

    ./rlgl e -l project=libffi -l sha=${GITHUB_SHA:0:7} -l CC="${HOST}-gcc" -l host=$HOST --policy=https://github.com/libffi/rlgl-policy.git */testsuite/libffi.log
    exit $?
}

function build_ios()
{
    which python
    # export PYTHON_BIN=/usr/local/bin/python
    ./generate-darwin-source-and-headers.py --only-ios
    xcodebuild -showsdks
    xcodebuild -project libffi.xcodeproj -target "libffi-iOS" -configuration Release -sdk iphoneos11.4
    exit $?
}

function build_macosx()
{
    which python
    # export PYTHON_BIN=/usr/local/bin/python
    ./generate-darwin-source-and-headers.py --only-osx
    xcodebuild -showsdks
    xcodebuild -project libffi.xcodeproj -target "libffi-Mac" -configuration Release -sdk macosx10.13
    echo "Finished build"
    exit $?
}

# QEMU jobs select their build method by env var (set in the workflow), which
# avoids fragile HOST-triple glob matching:
#   FOREIGN_IMAGE -> native build+test inside a QEMU-run foreign-arch container
#   CROSS_QEMU    -> cross-compile + run tests under qemu-user (binfmt)
if [ -n "${FOREIGN_IMAGE:-}" ]; then
    ./autogen.sh
    build_foreign_linux "$HOST" "$FOREIGN_IMAGE"
fi
if [ -n "${CROSS_QEMU:-}" ]; then
    ./autogen.sh
    build_cross_qemu
fi

case "$HOST" in
    arm-apple-darwin*)
	      ./autogen.sh
	      build_ios
	      ;;
    x86_64-apple-darwin*)
	      ./autogen.sh
	      build_macosx
	      ;;
    arm32v7-linux-gnu)
	      ./autogen.sh
        build_foreign_linux arm quay.io/moxielogic/arm32v7-ci-build-container:latest
	      ;;
    bfin-elf )
	      ./autogen.sh
	      GCC_OPTIONS=-msim build_cross
	      ;;
    m32r-elf )
	      ./autogen.sh
	      build_cross
	      ;;
    or1k-elf )
	      ./autogen.sh
	      build_cross
	      ;;
    powerpc-eabisim )
	      ./autogen.sh
	      build_cross
	      ;;
    m68k-linux-gnu )
	      ./autogen.sh
	      GCC_OPTIONS=-mcpu=547x build_cross_linux
	      ;;
    alpha-linux-gnu | sh4-linux-gnu )
	      ./autogen.sh
	      build_cross_linux
	      ;;
    *)
	      ./autogen.sh
	      build_linux
	      ;;
esac

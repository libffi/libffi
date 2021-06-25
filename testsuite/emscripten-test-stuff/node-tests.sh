#!/bin/bash

# JS BigInt to Wasm i64 integration, disabled by default
WASM_BIGINT=false

# Parse arguments
while [ $# -gt 0 ]; do
  case $1 in
    --enable-wasm-bigint) WASM_BIGINT=true ;;
    *) echo "ERROR: Unknown parameter: $1" >&2; exit 1 ;;
  esac
  shift
done

# Common compiler flags
export CFLAGS="-O3 -fPIC"
if [ "$WASM_BIGINT" = "true" ]; then
  # We need to detect WASM_BIGINT support at compile time
  export CFLAGS+=" -DWASM_BIGINT";
fi
export CXXFLAGS="$CFLAGS"
export LDFLAGS="-O3"
if [ "$WASM_BIGINT" = "true" ]; then
  export LDFLAGS+=" -s WASM_BIGINT";
else
  export LDFLAGS+=" -s DYNCALLS";
fi

# Specific variables for cross-compilation
export CHOST="wasm32-unknown-linux" # wasm32-unknown-emscripten

autoreconf -fiv
emconfigure ./configure --host=$CHOST --enable-static --disable-shared \
  --disable-builddir --disable-multi-os-directory --disable-raw-api || (cat config.log && exit 1)
make
EMMAKEN_JUST_CONFIGURE=1 emmake make check \
  RUNTESTFLAGS="LDFLAGS_FOR_TARGET='$LDFLAGS'" || (cat testsuite/libffi.log && exit 1)

#!/bin/bash
SOURCE_DIR=$PWD

# Working directories
TARGET=$SOURCE_DIR/target

# Parse arguments
while [ $# -gt 0 ]; do
  case $1 in
    --enable-wasm-bigint) WASM_BIGINT_FLAG="-s WASM_BIGINT" ;;
    *) echo "ERROR: Unknown parameter: $1" >&2; exit 1 ;;
  esac
  shift
done

# Common compiler flags
export CFLAGS="-O3 -fPIC"
if [ -n "$WASM_BIGINT_FLAG" ]; then
  export CFLAGS="$CFLAGS $WASM_BIGINT_FLAG -DWASM_BIGINT"
fi
export CXXFLAGS="$CFLAGS"
export LDFLAGS="-L$TARGET/lib -O3"

# Build paths
export CPATH="$TARGET/include"
export PKG_CONFIG_PATH="$TARGET/lib/pkgconfig"
export EM_PKG_CONFIG_PATH="$PKG_CONFIG_PATH"

# Specific variables for cross-compilation
export CHOST="wasm32-unknown-linux" # wasm32-unknown-emscripten

autoreconf -fiv
emconfigure ./configure --host=wasm32-unknown-linux --enable-static \
  --disable-shared  --disable-builddir --disable-multi-os-directory \
  --disable-raw-api
make
EMMAKEN_JUST_CONFIGURE=1 emmake make check \
  RUNTESTFLAGS="LDFLAGS_FOR_TARGET='\
    -s DYNCALLS \
    -s EXPORTED_RUNTIME_METHODS='getTempRet0' \
    -s EXPORTED_RUNTIME_METHODS='stackSave' \
    -s EXPORTED_RUNTIME_METHODS='stackRestore' \
    '"
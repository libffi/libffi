#!/usr/bin/env bash
command -v emcc >/dev/null 2>&1 || { echo >&2 "emsdk could not be found.  Aborting."; exit 1; }

set -e

SOURCE_DIR=$PWD

# Working directories
TARGET=$SOURCE_DIR/target
mkdir -p $TARGET

# Define default arguments

# JS BigInt to Wasm i64 integration, disabled by default
WASM_BIGINT=false

# Parse arguments
while [ $# -gt 0 ]; do
  case $1 in
    --enable-wasm-bigint) WASM_BIGINT=true ;;
    --pyodide-fpcast) PYODIDE_FPCAST=true ;;
    --debug) DEBUG=true ;;
    *) echo "ERROR: Unknown parameter: $1" >&2; exit 1 ;;
  esac
  shift
done

# Common compiler flags
export CFLAGS="-O3 -fPIC"
if [ "$WASM_BIGINT" = "true" ]; then
  # We need to detect WASM_BIGINT support at compile time
  export CFLAGS+=" -DWASM_BIGINT"
fi

if [ "$PYODIDE_FPCAST" = "true" ]; then
  export CFLAGS+=" -DPYODIDE_FPCAST"
fi

if [ "$DEBUG" = "true" ]; then
  export CFLAGS+=" -DDEBUG_F"
fi

export CXXFLAGS="$CFLAGS"
export LDFLAGS="-L$TARGET/lib -O3 -s EXPORTED_FUNCTIONS=['_main','_malloc','_free'] -s ALLOW_TABLE_GROWTH"
if [ "$WASM_BIGINT" = "true" ]; then
  export LDFLAGS+=" -s WASM_BIGINT"
fi

# Build paths
export CPATH="$TARGET/include"
export PKG_CONFIG_PATH="$TARGET/lib/pkgconfig"
export EM_PKG_CONFIG_PATH="$PKG_CONFIG_PATH"

# Specific variables for cross-compilation
export CHOST="wasm32-unknown-linux" # wasm32-unknown-emscripten

autoreconf -fiv
emconfigure ./configure --host=$CHOST --prefix=$TARGET --enable-static --disable-shared --disable-dependency-tracking \
  --disable-builddir --disable-multi-os-directory --disable-raw-api --disable-docs
make install
cp fficonfig.h target/include/

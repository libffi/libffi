#!/bin/sh
set -e
cd "$1"
export CFLAGS="-fPIC -I../../target/include $EXTRA_CFLAGS"
export LDFLAGS=" \
    -sEXPORT_ALL \
    -sMODULARIZE \
    -sMAIN_MODULE \
    -L../../target/lib/ -lffi \
    -sEXPORTED_RUNTIME_METHODS='getTempRet0' \
    -sSTRICT_JS \
    $EXTRA_LD_FLAGS \
"

# Rename main functions to test__filename so we can link them together
ls *.c | sed 's!\(.*\)\.c!sed -i "s/main/test__\1/g" \0!g' | bash

# Compile
ls *.c | sed 's/\(.*\)\.c/emcc $CFLAGS -c \1.c -o \1.o /g' | bash

# Link
em++ $LDFLAGS *.o -o test.js
cp ../emscripten-test-stuff/test.html .

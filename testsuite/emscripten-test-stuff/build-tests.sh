#!/bin/sh
cd $1
export CFLAGS="-fPIC -I../.. -I ../../target/include -I ../../include/ $EXTRA_CFLAGS"
export LDFLAGS=" \
    -s EXPORT_ALL=1 \
    -s MODULARIZE=1 \
    -s MAIN_MODULE=1 \
    -L../../target/lib/ -lffi 
    -s EXPORTED_RUNTIME_METHODS='getTempRet0' \
    -s EXPORTED_RUNTIME_METHODS='stackSave' \
    -s EXPORTED_RUNTIME_METHODS='stackRestore' \
    $EXTRA_LD_FLAGS
    "
# Rename main functions to test__filename so we can link them together
ls *.c | sed 's!\(.*\)\.c!sed -i "s/main/test__\1/g" \0!g' | bash

# Compile
ls *.c | sed 's/\(.*\)\.c/emcc $CFLAGS -c \1.c -o \1.o /g' | bash

# Link
em++ $LDFLAGS *.o -o test.js
cp ../emscripten-test-stuff/test.html .
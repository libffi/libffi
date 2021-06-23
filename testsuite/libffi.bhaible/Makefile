CC = emcc
CPP = em++
CFLAGS = -O2 -Wall -fPIC
prefix = ../../target
includedir = $(prefix)/include
libdir = $(prefix)/lib
CPPFLAGS = -I$(includedir)
LDFLAGS=-s EXPORT_ALL=1 -s MODULARIZE=1 -s MAIN_MODULE=1 -s EXPORTED_RUNTIME_METHODS='["getTempRet0"]'

all: check-call check-callback

test-call: test-call.c testcases.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -o test-call.o -c test-call.c
	$(CPP) $(LDFLAGS) -L$(libdir) -lffi test-call.o -o test-call.js

test-callback: test-callback.c testcases.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o test-callback test-callback.c -lffi

check-call: test-call
	./test-call > test-call.out
	LC_ALL=C uniq -u < test-call.out > failed-call
	test '!' -s failed-call

check-callback: test-callback
	./test-callback > test-callback.out
	LC_ALL=C uniq -u < test-callback.out > failed-callback
	test '!' -s failed-callback

clean:
	rm -f test-call test-callback test-call.out test-callback.out failed-call failed-callback

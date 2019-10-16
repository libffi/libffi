/* Area:        ffi_call
   Purpose:     Check strlen function call with additional arguments.
   Limitations: none.
   PR:          none.
   Originator:  From the original ffitest.c  */

/* { dg-do run } */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cstdint>

#include <ffi.h>

#define MAX_ARGS 256
#define CHECK(x) (void)(!(x) ? (abort(), 1) : 0)

#ifndef ABI_NUM
#define ABI_NUM FFI_DEFAULT_ABI
#define ABI_ATTR
#endif

static size_t ABI_ATTR my_f(char *s, float a)
{
    return (size_t) ((int) strlen(s) + (int) a);
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, std::size_t size) {

    ffi_cif cif;
    ffi_type *args[MAX_ARGS];
    void *values[MAX_ARGS];
    ffi_arg rint;
    const char *s;
    float v2;
    args[0] = &ffi_type_pointer;
    args[1] = &ffi_type_float;
    values[0] = (void*) &s;
    values[1] = (void*) &v2;

    /* Initialize the cif */
    CHECK(ffi_prep_cif(&cif, ABI_NUM, 2,
                       &ffi_type_sint, args) == FFI_OK);

    s = (char *)data;
    v2 = (float)size;
    ffi_call(&cif, FFI_FN(my_f), &rint, values);

    return 0;
}

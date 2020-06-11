/* Area:	ffi_call
   Purpose:	Check strlen function call.
   Limitations:	none.
   PR:		none.
   Originator:	From the original ffitest.c  */

/* { dg-do run } */

#include <cstdint>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ffi.h>

#define MAX_ARGS 256
#define CHECK(x) (void)(!(x) ? (abort(), 1) : 0)

#ifndef ABI_NUM
#define ABI_NUM FFI_DEFAULT_ABI
#define ABI_ATTR
#endif

static size_t ABI_ATTR my_strlen(char *s)
{
  return (strlen(s));
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *Data, std::size_t Size) {
  ffi_cif cif;
  ffi_type *args[MAX_ARGS];
  void *values[MAX_ARGS];
  ffi_arg rint;
  char *s;

  args[0] = &ffi_type_pointer;
  values[0] = (void*) &s;
  
  /* Initialize the cif */
  CHECK(ffi_prep_cif(&cif, ABI_NUM, 1,
		     &ffi_type_sint, args) == FFI_OK);
  s = (char *)Data;
  ffi_call(&cif, FFI_FN(my_strlen), &rint, values);
  
  return 0;
}
  

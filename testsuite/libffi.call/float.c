/* Area:	ffi_call
   Purpose:	Check return value float.
   Limitations:	none.
   PR:		none.
   Originator:	From the original ffitest.c  */

/* { dg-do run } */

#include "ffitest.h"
#include <emscripten/emscripten.h>


static int floating(long double d)
{
  return (int)d;
}

int main (void)
{
  ffi_cif cif;
  ffi_type *args[MAX_ARGS];
  void *values[MAX_ARGS];
  ffi_arg rint;

  float f;
  signed int si1;
  double d;
  long double ld;

  args[0] = &ffi_type_longdouble;
  values[0] = &ld;

  /* Initialize the cif */
  CHECK(ffi_prep_cif(&cif, FFI_DEFAULT_ABI, 1,
		     &ffi_type_sint, args) == FFI_OK);

  si1 = 6;
  f = 3.14159;
  d = (double)1.0/(double)3.0;
  ld = 2.71828182846L;

  floating (ld);

  ffi_call(&cif, FFI_FN(floating), &rint, values);

  printf ("%d vs %d\n", (int)rint, floating (ld));

  CHECK((int)rint == floating(ld));

  ;
}

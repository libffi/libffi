/* Area:	closure_call
   Purpose:	Call a closure taking _Complex double arguments from a
		caller that passes every argument in registers.
   Limitations:	none.
   PR:		none.
   Originator:	powerpc64 ELFv2 parameter save area regression.

   On powerpc64 ELFv2 the caller allocates a parameter save area only if
   some argument is passed in memory.  The three complex values here go in
   FPRs; only their shadow doublewords run past the eighth.  libffi assumed
   a save area anyway, and the closure entry stored r3-r10 over the
   caller's frame, replacing the saved return address of call_it.  */

/* { dg-do run } */
#include "ffitest.h"
#include <complex.h>

typedef double (*callback) (void *, long, long, _Complex double,
			    _Complex double, _Complex double);

static void
callback_gn (ffi_cif *cif __UNUSED__, void *resp, void **args,
	     void *userdata __UNUSED__)
{
  long channel = *(long *) args[1];
  long tag = *(long *) args[2];
  _Complex double a = *(_Complex double *) args[3];
  _Complex double b = *(_Complex double *) args[4];
  _Complex double c = *(_Complex double *) args[5];

  *(double *) resp = channel * 1000 + tag * 100
		     + creal (a) + cimag (a) * 2 + creal (b) * 3
		     + cimag (b) * 4 + creal (c) * 5 + cimag (c) * 6;
}

static int context;

/* Makes no other call, so it has no parameter save area of its own; the
   addition keeps the call from becoming a tail call.  */
static double __attribute__ ((noinline))
call_it (callback fn)
{
  return fn (&context, 7, 3, 1 + 2 * I, 3 + 4 * I, 5 + 6 * I) + 1;
}

int main (void)
{
  ffi_cif cif;
  void *code;
  ffi_closure *pcl = ffi_closure_alloc (sizeof (ffi_closure), &code);
  ffi_type *args[6];

  args[0] = &ffi_type_pointer;
  args[1] = &ffi_type_slong;
  args[2] = &ffi_type_slong;
  args[3] = args[4] = args[5] = &ffi_type_complex_double;

  CHECK(ffi_prep_cif(&cif, FFI_DEFAULT_ABI, 6, &ffi_type_double, args) == FFI_OK);
  CHECK(ffi_prep_closure_loc(pcl, &cif, callback_gn, NULL, code) == FFI_OK);

  /* 7000 + 300 + 1 + 4 + 9 + 16 + 25 + 36, plus call_it's 1.  */
  CHECK(call_it ((callback) code) == 7392);

  ffi_closure_free (pcl);
  exit(0);
}

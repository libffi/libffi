/* Area:	ffi_prep_raw_closure_loc
   Purpose:	Check that a raw-API closure allocated with ffi_closure_alloc
		is invoked correctly.  With static trampolines the executable
		code address differs from the closure data, and the raw API
		passed the wrong one as the handler's user_data, so the handler
		was read from read-only trampoline bytes and the closure made a
		wild indirect call (GHSA-w6j9-w53f-2857).  There was previously
		no test for ffi_prep_raw_closure*.
   Limitations:	none.
   PR:		GHSA-w6j9-w53f-2857.  */

/* { dg-do run } */
#include "ffitest.h"

#if FFI_CLOSURES

static void
raw_handler (ffi_cif *cif, void *ret, ffi_raw *args, void *ud)
{
  (void) cif;
  *(ffi_arg *) ret = (ffi_arg) (args[0].sint + args[1].sint + (long) ud);
}

int
main (void)
{
  ffi_cif cif;
  ffi_type *args[2] = { &ffi_type_sint, &ffi_type_sint };
  ffi_raw_closure *cl;
  void *code;
  int (*f) (int, int);

  CHECK (ffi_prep_cif (&cif, FFI_DEFAULT_ABI, 2, &ffi_type_sint, args)
	 == FFI_OK);

  cl = ffi_closure_alloc (sizeof (ffi_raw_closure), &code);
  CHECK (cl != NULL);
  CHECK (ffi_prep_raw_closure_loc (cl, &cif, raw_handler, (void *) 100, code)
	 == FFI_OK);

  f = (int (*) (int, int)) code;
  CHECK (f (20, 3) == 123);

  ffi_closure_free (cl);
  printf ("raw closure ok\n");
  exit (0);
}

#else
int main (void) { return 0; }
#endif

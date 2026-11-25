/* Area:	ffi_call, closure_call
   Purpose:	Check integer complex types of one and two registers.
   Limitations:	none.
   PR:		none.  */

/* { dg-do run } */

#include "ffitest.h"
#include <complex.h>

/* _Complex int and _Complex long long are returned in one or two
   integer registers (or more on 32-bit targets), which for complex
   values is a different code path from the floating-point types.  The
   second function takes a double first, so that on targets which pass
   leading floating-point arguments in FP registers the closure has to
   keep them apart from the result.  */

#define FFI_COMPLEX_TYPEDEF(name, type, ffitype)	     \
  static ffi_type *ffi_elements_complex_##name [2] = {	     \
    (ffi_type *)(&ffitype), NULL			     \
  };							     \
  struct struct_align_complex_##name {			     \
    char c;						     \
    _Complex type x;					     \
  };							     \
  static ffi_type ffi_type_complex_##name = {		     \
    sizeof(_Complex type),				     \
    offsetof(struct struct_align_complex_##name, x),	     \
    FFI_TYPE_COMPLEX,					     \
    (ffi_type **)ffi_elements_complex_##name		     \
  }

FFI_COMPLEX_TYPEDEF(sint, int, ffi_type_sint);
FFI_COMPLEX_TYPEDEF(sint64, long long, ffi_type_sint64);

typedef _Complex int ci;
typedef _Complex long long cll;

static ci
f_ci (ci a, int k)
{
  __real__ a = __real__ a * k;
  __imag__ a = __imag__ a - k;
  return a;
}

static cll
f_cll (double d, cll a)
{
  __real__ a = __real__ a * (long long) d;
  __imag__ a = __imag__ a - (long long) d;
  return a;
}

static void
cls_ci (ffi_cif *cif __UNUSED__, void *resp, void **args,
	void *userdata __UNUSED__)
{
  *(ci *) resp = f_ci (*(ci *) args[0], *(int *) args[1]);
}

static void
cls_cll (ffi_cif *cif __UNUSED__, void *resp, void **args,
	 void *userdata __UNUSED__)
{
  *(cll *) resp = f_cll (*(double *) args[0], *(cll *) args[1]);
}

int
main (void)
{
  ffi_cif cif;
  ffi_type *types[2];
  void *values[2];
  ffi_closure *pcl;
  void *code;
  ci a, ra;
  cll b, rb;
  int k = 3;
  double d = 5.0;

  pcl = ffi_closure_alloc (sizeof (ffi_closure), &code);
  CHECK (pcl != NULL);

  __real__ a = 5;
  __imag__ a = -7;
  __real__ b = 0x100000003LL;
  __imag__ b = -0x400000007LL;

  types[0] = &ffi_type_complex_sint;
  types[1] = &ffi_type_sint;
  values[0] = &a;
  values[1] = &k;
  CHECK (ffi_prep_cif (&cif, FFI_DEFAULT_ABI, 2, &ffi_type_complex_sint,
		       types) == FFI_OK);
  ffi_call (&cif, FFI_FN (f_ci), &ra, values);
  printf ("ci: %d,%di\n", (int) __real__ ra, (int) __imag__ ra);
  CHECK (__real__ ra == 15 && __imag__ ra == -10);
  CHECK (ffi_prep_closure_loc (pcl, &cif, cls_ci, NULL, code) == FFI_OK);
  ra = ((ci (*)(ci, int)) code) (a, k);
  printf ("closure ci: %d,%di\n", (int) __real__ ra, (int) __imag__ ra);
  CHECK (__real__ ra == 15 && __imag__ ra == -10);

  types[0] = &ffi_type_double;
  types[1] = &ffi_type_complex_sint64;
  values[0] = &d;
  values[1] = &b;
  CHECK (ffi_prep_cif (&cif, FFI_DEFAULT_ABI, 2, &ffi_type_complex_sint64,
		       types) == FFI_OK);
  ffi_call (&cif, FFI_FN (f_cll), &rb, values);
  printf ("cll: %lld,%lldi\n", (long long) __real__ rb,
	  (long long) __imag__ rb);
  CHECK (__real__ rb == 0x50000000FLL && __imag__ rb == -0x40000000CLL);
  CHECK (ffi_prep_closure_loc (pcl, &cif, cls_cll, NULL, code) == FFI_OK);
  rb = ((cll (*)(double, cll)) code) (d, b);
  printf ("closure cll: %lld,%lldi\n", (long long) __real__ rb,
	  (long long) __imag__ rb);
  CHECK (__real__ rb == 0x50000000FLL && __imag__ rb == -0x40000000CLL);

  ffi_closure_free (pcl);
  exit (0);
}

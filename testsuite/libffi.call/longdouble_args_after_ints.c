/* Area:	ffi_call
   Purpose:	Check long double arguments after an odd and an even number
		of ints.
   Limitations:	none.
   PR:		none.  */

/* { dg-do run } */

#include "ffitest.h"

/* Where long double needs 16-byte alignment, as on mips64 n64, an odd
   number of preceding ints leaves a gap before it.  With enough
   arguments, the long double or the int after it goes to the stack.  */

static double ABI_ATTR
f5 (int a, int b, int c, int d, int e, long double x, int k)
{
  return a + b + c + d + e + (double) x * k;
}

static double ABI_ATTR
f6 (int a, int b, int c, int d, int e, int f, long double x, int k)
{
  return a + b + c + d + e + f + (double) x * k;
}

static double ABI_ATTR
f7 (int a, int b, int c, int d, int e, int f, int g, long double x, int k)
{
  return a + b + c + d + e + f + g + (double) x * k;
}

int
main (void)
{
  ffi_type *types[9];
  void *values[9];
  int ints[7] = { 1, 2, 3, 4, 5, 6, 7 };
  int k = 10, n;
  long double x = 2.5L;
  ffi_cif cif;
  double r;

  for (n = 5; n <= 7; n++)
    {
      int i;

      for (i = 0; i < n; i++)
	{
	  types[i] = &ffi_type_sint;
	  values[i] = &ints[i];
	}
      types[n] = &ffi_type_longdouble;
      values[n] = &x;
      types[n + 1] = &ffi_type_sint;
      values[n + 1] = &k;

      CHECK (ffi_prep_cif (&cif, ABI_NUM, n + 2, &ffi_type_double, types)
	     == FFI_OK);
      ffi_call (&cif, n == 5 ? FFI_FN (f5) : n == 6 ? FFI_FN (f6)
		: FFI_FN (f7), &r, values);
      printf ("%d ints: %g\n", n, r);
      CHECK (r == (n == 5 ? f5 (1, 2, 3, 4, 5, x, k)
		   : n == 6 ? f6 (1, 2, 3, 4, 5, 6, x, k)
		   : f7 (1, 2, 3, 4, 5, 6, 7, x, k)));
    }

  exit (0);
}

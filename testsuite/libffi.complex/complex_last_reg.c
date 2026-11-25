/* Area:	ffi_call, closure_call
   Purpose:	Pass a float, double or long double _Complex that starts in
		the last argument register slots it could use, followed by
		more arguments.
   Limitations:	none.
   PR:		none.

   On MIPS N32/N64 a complex value goes in FP registers only if all of it
   fits; one that starts in the eighth slot (or, for long double, the
   seventh pair) is passed like a struct, in the remaining GPR and on the
   stack.  ffi_prep_args expanded a float _Complex there to two slots and
   ran past the argument area, storing later arguments into
   ffi_call_N32's own frame, and closures read the value from past the
   saved FP registers.  */

/* { dg-do run } */

#include "ffitest.h"
#include <complex.h>

#define WEIGH(a1, a2, a3, a4, a5, a6, b1, b2, b3, b4, b5, b6)		\
  (a1 + a2 * 2 + a3 * 3 + a4 * 5 + a5 * 7 + a6 * 11			\
   + b1 * 23 + b2 * 29 + b3 * 31 + b4 * 37 + b5 * 41 + b6 * 43)

/* Seven longs, so the complex value starts in the eighth slot.  */
#define DEFINE_SEVEN(name, ctype, real, imag)				\
  typedef long (*name##_fn) (long, long, long, long, long, long, long,	\
			     _Complex ctype, long, long, long, long, long, \
			     long);					\
  static long								\
  name (long a1, long a2, long a3, long a4, long a5, long a6, long a7,	\
	_Complex ctype z, long b1, long b2, long b3, long b4, long b5,	\
	long b6)							\
  {									\
    return WEIGH (a1, a2, a3, a4, a5, a6, b1, b2, b3, b4, b5, b6)	\
	   + a7 * 13 + (long) real (z) * 17 + (long) imag (z) * 19;	\
  }									\
  static void								\
  name##_gn (ffi_cif *cif __UNUSED__, void *resp, void **args,		\
	     void *userdata __UNUSED__)					\
  {									\
    long *a[14];							\
    int i;								\
    for (i = 0; i < 14; i++)						\
      a[i] = (long *) args[i];						\
    *(ffi_arg *) resp							\
      = name (*a[0], *a[1], *a[2], *a[3], *a[4], *a[5], *a[6],		\
	      *(_Complex ctype *) args[7], *a[8], *a[9], *a[10], *a[11],	\
	      *a[12], *a[13]);						\
  }

DEFINE_SEVEN (f_float, float, crealf, cimagf)
DEFINE_SEVEN (f_double, double, creal, cimag)

/* Six longs, so a long double _Complex (which takes an aligned pair of
   pairs) starts in the seventh slot.  */
typedef long (*f_ld_fn) (long, long, long, long, long, long,
			 _Complex long double, long, long, long, long, long,
			 long);

static long
f_ld (long a1, long a2, long a3, long a4, long a5, long a6,
      _Complex long double z, long b1, long b2, long b3, long b4, long b5,
      long b6)
{
  return WEIGH (a1, a2, a3, a4, a5, a6, b1, b2, b3, b4, b5, b6)
	 + (long) creall (z) * 17 + (long) cimagl (z) * 19;
}

static void
f_ld_gn (ffi_cif *cif __UNUSED__, void *resp, void **args,
	 void *userdata __UNUSED__)
{
  long *a[13];
  int i;

  for (i = 0; i < 13; i++)
    a[i] = (long *) args[i];
  *(ffi_arg *) resp = f_ld (*a[0], *a[1], *a[2], *a[3], *a[4], *a[5],
			    *(_Complex long double *) args[6], *a[7], *a[8],
			    *a[9], *a[10], *a[11], *a[12]);
}

static long l[13];

/* Build a cif of NLONGS longs, the complex type CTYPE, then six longs, and
   check it through ffi_call and a closure against EXPECTED.  */
static void
check (int nlongs, ffi_type *ctype, void *z, void (*fn) (void),
       void (*gn) (ffi_cif *, void *, void **, void *), long expected,
       void **code_out, ffi_closure **pcl_out, ffi_cif *cif)
{
  static ffi_type *args[14];
  static void *values[14];
  ffi_arg res;
  int i, j;

  for (i = 0, j = 0; i < nlongs + 7; i++)
    {
      if (i == nlongs)
	{
	  args[i] = ctype;
	  values[i] = z;
	  continue;
	}
      args[i] = &ffi_type_slong;
      values[i] = &l[j++];
    }

  CHECK (ffi_prep_cif (cif, FFI_DEFAULT_ABI, nlongs + 7, &ffi_type_slong,
		       args) == FFI_OK);
  res = 0;
  ffi_call (cif, fn, &res, values);
  CHECK ((long) res == expected);

  *pcl_out = ffi_closure_alloc (sizeof (ffi_closure), code_out);
  CHECK (*pcl_out != NULL);
  CHECK (ffi_prep_closure_loc (*pcl_out, cif, gn, NULL, *code_out)
	 == FFI_OK);
}

int
main (void)
{
  _Complex float zf = 3.0f + 4.0f * I;
  _Complex double zd = 5.0 + 6.0 * I;
  _Complex long double zl = 7.0L + 8.0L * I;
  ffi_cif cif_f, cif_d, cif_l;
  ffi_closure *pcl;
  void *code;
  long expected;
  int i;

  for (i = 0; i < 13; i++)
    l[i] = 100 + i;

  expected = f_float (l[0], l[1], l[2], l[3], l[4], l[5], l[6], zf, l[7],
		      l[8], l[9], l[10], l[11], l[12]);
  check (7, &ffi_type_complex_float, &zf, FFI_FN (f_float), f_float_gn,
	 expected, &code, &pcl, &cif_f);
  CHECK (((f_float_fn) code) (l[0], l[1], l[2], l[3], l[4], l[5], l[6], zf,
			      l[7], l[8], l[9], l[10], l[11], l[12])
	 == expected);
  ffi_closure_free (pcl);

  expected = f_double (l[0], l[1], l[2], l[3], l[4], l[5], l[6], zd, l[7],
		       l[8], l[9], l[10], l[11], l[12]);
  check (7, &ffi_type_complex_double, &zd, FFI_FN (f_double), f_double_gn,
	 expected, &code, &pcl, &cif_d);
  CHECK (((f_double_fn) code) (l[0], l[1], l[2], l[3], l[4], l[5], l[6], zd,
			       l[7], l[8], l[9], l[10], l[11], l[12])
	 == expected);
  ffi_closure_free (pcl);

  expected = f_ld (l[0], l[1], l[2], l[3], l[4], l[5], zl, l[6], l[7], l[8],
		   l[9], l[10], l[11]);
  check (6, &ffi_type_complex_longdouble, &zl, FFI_FN (f_ld), f_ld_gn,
	 expected, &code, &pcl, &cif_l);
  CHECK (((f_ld_fn) code) (l[0], l[1], l[2], l[3], l[4], l[5], zl, l[6],
			   l[7], l[8], l[9], l[10], l[11]) == expected);
  ffi_closure_free (pcl);

  exit (0);
}

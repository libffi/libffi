/* Area:		ffi_call, closure_call
   Purpose:		Check that integer arguments following a homogeneous
			aggregate of long double are not shifted by a
			doubleword (PowerPC64 ELFv2), and that a closure whose
			arguments then fit in eight doublewords does not write
			into its caller's frame.
   Limitations:	none.
   PR:			none.
   Originator:		none.  */

/* { dg-do run } */
/* { dg-options -mlong-double-128 { target ppc64_longdouble128 } } */

#include "ffitest.h"

typedef struct ld_pair {
  long double a;
  long double b;
} ld_pair;

static long
ld_pair_fn (long n, ld_pair p, long m)
{
  printf ("%ld %Lg %Lg %ld\n", n, p.a, p.b, m);
  CHECK (n == 1);
  CHECK (p.a == 2);
  CHECK (p.b == 3);
  CHECK (m == 4);
  return n + m;
}

static void
ld_pair_gn (ffi_cif *cif __UNUSED__, void *resp,
	    void **args, void *userdata __UNUSED__)
{
  long n = *(long *) args[0];
  ld_pair p = *(ld_pair *) args[1];
  long m = *(long *) args[2];

  *(ffi_arg *) resp = ld_pair_fn (n, p, m);
}

/* (long, long, long, ld_pair, long) fits in eight doublewords, so an
   ELFv2 caller allocates no parameter save area for it.  */
typedef long (*five_fn) (long, long, long, ld_pair, long);

static void
five_gn (ffi_cif *cif __UNUSED__, void *resp,
	 void **args, void *userdata __UNUSED__)
{
  ld_pair p = *(ld_pair *) args[3];

  *(ffi_arg *) resp = *(long *) args[0] + *(long *) args[1] * 2
		      + *(long *) args[2] * 3 + (long) (p.a * 5 + p.b * 7)
		      + *(long *) args[4] * 11;
}

/* Makes no other call, so it has no parameter save area of its own; the
   addition keeps the call from becoming a tail call.  */
static long __attribute__ ((noinline))
call_five (five_fn fn, ld_pair p)
{
  return fn (1, 2, 3, p, 4) + 1;
}

int
main (void)
{
  ffi_cif cif;
  void *code;
  ffi_closure *pcl = ffi_closure_alloc (sizeof (ffi_closure), &code);
  ffi_type *ld_pair_fields[3];
  ffi_type ld_pair_type;
  ffi_type *arg_types[4];
  void *args[4];
  ffi_arg res;
  long n = 1, m = 4;
  ld_pair p = { 2, 3 };

  ld_pair_type.size = 0;
  ld_pair_type.alignment = 0;
  ld_pair_type.type = FFI_TYPE_STRUCT;
  ld_pair_type.elements = ld_pair_fields;
  ld_pair_fields[0] = &ffi_type_longdouble;
  ld_pair_fields[1] = &ffi_type_longdouble;
  ld_pair_fields[2] = NULL;

  arg_types[0] = &ffi_type_slong;
  arg_types[1] = &ld_pair_type;
  arg_types[2] = &ffi_type_slong;
  arg_types[3] = NULL;

  CHECK (ffi_prep_cif (&cif, FFI_DEFAULT_ABI, 3, &ffi_type_slong,
		       arg_types) == FFI_OK);

  args[0] = &n;
  args[1] = &p;
  args[2] = &m;
  args[3] = NULL;

  res = 0;
  ffi_call (&cif, FFI_FN (ld_pair_fn), &res, args);
  /* { dg-output "1 2 3 4" } */
  printf ("res: %d\n", (int) res);
  /* { dg-output "\nres: 5" } */
  CHECK (res == 5);

  CHECK (ffi_prep_closure_loc (pcl, &cif, ld_pair_gn, NULL, code) == FFI_OK);

  res = ((long (*) (long, ld_pair, long)) code) (n, p, m);
  /* { dg-output "\n1 2 3 4" } */
  printf ("res: %d\n", (int) res);
  /* { dg-output "\nres: 5" } */
  CHECK (res == 5);

  {
    ffi_closure *five = ffi_closure_alloc (sizeof (ffi_closure), &code);
    ffi_type *five_types[5] = { &ffi_type_slong, &ffi_type_slong,
				&ffi_type_slong, &ld_pair_type,
				&ffi_type_slong };
    ffi_cif five_cif;

    CHECK (ffi_prep_cif (&five_cif, FFI_DEFAULT_ABI, 5, &ffi_type_slong,
			 five_types) == FFI_OK);
    CHECK (ffi_prep_closure_loc (five, &five_cif, five_gn, NULL, code)
	   == FFI_OK);
    /* 1 + 4 + 9 + 10 + 21 + 44, plus call_five's 1.  */
    CHECK (call_five ((five_fn) code, p) == 90);
    ffi_closure_free (five);
  }

  ffi_closure_free (pcl);
  exit (0);
}

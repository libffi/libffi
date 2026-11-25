/* Area:	closure_call
   Purpose:	Receive structs holding a single double or float once all FP
		argument registers are in use.  On the hard-float RISC-V and
		LoongArch ABIs such a struct falls back to the integer calling
		convention (a GPR, then the stack) exactly like a bare double;
		the closure read it from past the end of the saved FP
		registers instead.
   Limitations:	none.
   PR:		none.
   Originator:	none  */

/* { dg-do run } */
#include "ffitest.h"

typedef struct { double d; } one_double;
typedef struct { float f; } one_float;

typedef double (*callback) (void *, double, double, double, double, double,
			    double, double, double, one_double, one_float,
			    one_double, one_float, one_double, one_float,
			    one_double, one_float, one_double, one_float);

static int marker = 42;

static void
callback_gn (ffi_cif *cif __UNUSED__, void *resp, void **args,
	     void *userdata __UNUSED__)
{
  double sum = 0;
  int i;

  CHECK(*(void **) args[0] == &marker);
  for (i = 0; i < 8; i++)
    sum += *(double *) args[1 + i] * (i + 1);
  for (i = 0; i < 10; i++)
    sum += (i & 1 ? ((one_float *) args[9 + i])->f
	    : ((one_double *) args[9 + i])->d) * (i + 9);
  *(double *) resp = sum;
}

static double __attribute__ ((noinline))
call_it (callback fn)
{
  one_double a = { 100 }, c = { 300 }, e = { 500 }, g = { 700 }, k = { 900 };
  one_float b = { 200 }, d = { 400 }, f = { 600 }, h = { 800 }, m = { 1000 };

  return fn (&marker, 1, 2, 3, 4, 5, 6, 7, 8, a, b, c, d, e, f, g, h, k, m);
}

int main (void)
{
  ffi_cif cif;
  void *code;
  ffi_closure *pcl = ffi_closure_alloc (sizeof (ffi_closure), &code);
  ffi_type *args[19];
  ffi_type double_type, float_type;
  ffi_type *double_elements[2], *float_elements[2];
  double expected = 0;
  int i;

  double_type.size = float_type.size = 0;
  double_type.alignment = float_type.alignment = 0;
  double_type.type = float_type.type = FFI_TYPE_STRUCT;
  double_elements[0] = &ffi_type_double;
  double_elements[1] = NULL;
  float_elements[0] = &ffi_type_float;
  float_elements[1] = NULL;
  double_type.elements = double_elements;
  float_type.elements = float_elements;

  args[0] = &ffi_type_pointer;
  for (i = 0; i < 8; i++)
    {
      args[1 + i] = &ffi_type_double;
      expected += (i + 1) * (i + 1);
    }
  for (i = 0; i < 10; i++)
    {
      args[9 + i] = i & 1 ? &float_type : &double_type;
      expected += 100 * (i + 1) * (i + 9);
    }

  CHECK(ffi_prep_cif(&cif, FFI_DEFAULT_ABI, 19, &ffi_type_double, args) == FFI_OK);
  CHECK(ffi_prep_closure_loc(pcl, &cif, callback_gn, NULL, code) == FFI_OK);

  CHECK(call_it ((callback) code) == expected);

  ffi_closure_free (pcl);
  exit(0);
}

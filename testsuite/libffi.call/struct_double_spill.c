/* Area:	ffi_call
   Purpose:	Check a struct holding a single double once all FP argument
		registers are in use.  On the hard-float RISC-V and LoongArch
		ABIs such a struct has to fall back to the integer calling
		convention (a GPR, then the stack) exactly like a bare double.
   Limitations:	none.
   PR:		none.
   Originator:	none  */

/* { dg-do run } */
#include "ffitest.h"

typedef struct
{
  double d;
} test_structure_d;

static double ABI_ATTR struct_double_spill(void *p,
					   double d0, double d1, double d2,
					   double d3, double d4, double d5,
					   double d6, double d7,
					   test_structure_d s0, test_structure_d s1,
					   test_structure_d s2, test_structure_d s3,
					   test_structure_d s4, test_structure_d s5,
					   test_structure_d s6, test_structure_d s7,
					   test_structure_d s8, test_structure_d s9)
{
  CHECK(*(int *)p == 42);
  return d0 + d1 + d2 + d3 + d4 + d5 + d6 + d7
    + s0.d + s1.d + s2.d + s3.d + s4.d + s5.d + s6.d + s7.d + s8.d + s9.d;
}

int main (void)
{
  ffi_cif cif;
  ffi_type *args[MAX_ARGS];
  void *values[MAX_ARGS];
  ffi_type ts_type;
  ffi_type *ts_type_elements[2];
  int marker = 42;
  void *p = &marker;
  double d[8];
  test_structure_d s[10];
  double rd, expected = 0;
  int i;

  ts_type.size = 0;
  ts_type.alignment = 0;
  ts_type.type = FFI_TYPE_STRUCT;
  ts_type.elements = ts_type_elements;
  ts_type_elements[0] = &ffi_type_double;
  ts_type_elements[1] = NULL;

  args[0] = &ffi_type_pointer;
  values[0] = &p;
  for (i = 0; i < 8; i++)
    {
      d[i] = i + 1;
      expected += d[i];
      args[1 + i] = &ffi_type_double;
      values[1 + i] = &d[i];
    }
  for (i = 0; i < 10; i++)
    {
      s[i].d = 100 * (i + 1);
      expected += s[i].d;
      args[9 + i] = &ts_type;
      values[9 + i] = &s[i];
    }

  /* Initialize the cif */
  CHECK(ffi_prep_cif(&cif, ABI_NUM, 19, &ffi_type_double, args) == FFI_OK);

  ffi_call(&cif, FFI_FN(struct_double_spill), &rd, values);

  CHECK_DOUBLE_EQ(rd, expected);

  exit(0);
}

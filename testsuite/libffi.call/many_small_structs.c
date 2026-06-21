/* Area:	ffi_call
   Purpose:	Pass many small (8-byte) structs by value.
   Limitations:	none.
   PR:		none.
   Originator:	secscan regression (PA-RISC 32-bit slot under-count).

   Regression test: on PA-RISC 32-bit, ffi_size_stack_pa32 reserved one stack
   slot per struct while ffi_prep_args_pa32 consumes two slots for a 5-8 byte
   struct passed inline, so a call with enough small-struct arguments wrote past
   the allocated argument frame.  Passing sixteen 8-byte structs by value must
   marshal correctly and return the expected sums.  */

/* { dg-do run } */
#include "ffitest.h"

typedef struct { int a; int b; } small_struct;

static small_struct ABI_ATTR
many_small (small_struct s0, small_struct s1, small_struct s2, small_struct s3,
	    small_struct s4, small_struct s5, small_struct s6, small_struct s7,
	    small_struct s8, small_struct s9, small_struct s10, small_struct s11,
	    small_struct s12, small_struct s13, small_struct s14, small_struct s15)
{
  small_struct r;
  r.a = s0.a + s1.a + s2.a + s3.a + s4.a + s5.a + s6.a + s7.a
      + s8.a + s9.a + s10.a + s11.a + s12.a + s13.a + s14.a + s15.a;
  r.b = s0.b + s1.b + s2.b + s3.b + s4.b + s5.b + s6.b + s7.b
      + s8.b + s9.b + s10.b + s11.b + s12.b + s13.b + s14.b + s15.b;
  return r;
}

#define NARGS 16

int main (void)
{
  ffi_cif cif;
  ffi_type *args[NARGS];
  void *values[NARGS];
  ffi_type ss_type;
  ffi_type *ss_elements[3];
  small_struct in[NARGS];
  small_struct result = { 0, 0 };
  int i, expected_a = 0, expected_b = 0;

  ss_type.size = 0;
  ss_type.alignment = 0;
  ss_type.type = FFI_TYPE_STRUCT;
  ss_type.elements = ss_elements;
  ss_elements[0] = &ffi_type_sint;
  ss_elements[1] = &ffi_type_sint;
  ss_elements[2] = NULL;

  for (i = 0; i < NARGS; i++)
    {
      in[i].a = i + 1;
      in[i].b = -(i + 1);
      expected_a += in[i].a;
      expected_b += in[i].b;
      args[i] = &ss_type;
      values[i] = &in[i];
    }

  CHECK(ffi_prep_cif(&cif, ABI_NUM, NARGS, &ss_type, args) == FFI_OK);

  ffi_call(&cif, FFI_FN(many_small), &result, values);

  CHECK(result.a == expected_a);
  CHECK(result.b == expected_b);

  exit(0);
}

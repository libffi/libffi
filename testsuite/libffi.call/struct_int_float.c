/* Area:	ffi_call
   Purpose:	Demonstrate structures with integers corrupting earlier floats
   Limitations:	none.
   PR:		#848
   Originator:	kellda  */

/* { dg-do run } */
#include "ffitest.h"

typedef struct
{
  unsigned long i;
  float f;
} test_structure_int_float;

static float ABI_ATTR struct_int_float(test_structure_int_float ts1,
                                       test_structure_int_float ts2,
                                       test_structure_int_float ts3,
                                       test_structure_int_float ts4,
                                       test_structure_int_float ts5,
                                       test_structure_int_float ts6)
{
  return ts1.f;
}

int main (void)
{
  ffi_cif cif;
  ffi_type *args[MAX_ARGS];
  void *values[MAX_ARGS];
  ffi_type ts_type;
  ffi_type *ts_type_elements[3];
  float rfloat;

  test_structure_int_float ts_arg[6];

  ts_type.size = 0;
  ts_type.alignment = 0;
  ts_type.type = FFI_TYPE_STRUCT;
  ts_type.elements = ts_type_elements;
  ts_type_elements[0] = &ffi_type_ulong;
  ts_type_elements[1] = &ffi_type_float;
  ts_type_elements[2] = NULL;

  args[0] = &ts_type;
  values[0] = &ts_arg[0];
  args[1] = &ts_type;
  values[1] = &ts_arg[1];
  args[2] = &ts_type;
  values[2] = &ts_arg[2];
  args[3] = &ts_type;
  values[3] = &ts_arg[3];
  args[4] = &ts_type;
  values[4] = &ts_arg[4];
  args[5] = &ts_type;
  values[5] = &ts_arg[5];
  
  /* Initialize the cif */
  CHECK(ffi_prep_cif(&cif, ABI_NUM, 6, &ffi_type_float, args) == FFI_OK);
  
  ts_arg[0].i = 1;
  ts_arg[0].f = 1.11f;
  ts_arg[1].i = 2;
  ts_arg[1].f = 2.22f;
  ts_arg[2].i = 3;
  ts_arg[2].f = 3.33f;
  ts_arg[3].i = 4;
  ts_arg[3].f = 4.44f;
  ts_arg[4].i = 5;
  ts_arg[4].f = 5.55f;
  ts_arg[5].i = 6;
  ts_arg[5].f = 6.66f;
  
  printf ("%g\n", ts_arg[0].f);
  printf ("%g\n", ts_arg[1].f);
  printf ("%g\n", ts_arg[2].f);
  printf ("%g\n", ts_arg[3].f);
  printf ("%g\n", ts_arg[4].f);
  printf ("%g\n", ts_arg[5].f);
  
  ffi_call(&cif, FFI_FN(struct_int_float), &rfloat, values);

  printf ("%g\n", rfloat);
  
  CHECK(rfloat == 1.11f);

  exit(0);
}

/* Area: ffi_call, closure_call
   Purpose: Check stack alignment and size for a mixed struct argument.
   Limitations: none.
   Issue: #750.  */

/* { dg-do run } */
#include "ffitest.h"

typedef struct
{
  float f1;
  float f2;
  int i;
} mixed_struct;

static int ABI_ATTR
struct_stack(long a1, long a2, long a3, long a4,
             long a5, long a6, long a7, long a8,
             int before, mixed_struct value, int after)
{
  return (int) (a1 + 2 * a2 + 3 * a3 + 4 * a4
                + 5 * a5 + 6 * a6 + 7 * a7 + 8 * a8)
    + before * 9 + (int) (value.f1 * 10) + (int) (value.f2 * 11)
    + value.i * 12 + after * 13;
}

#if FFI_CLOSURES
static void
struct_stack_closure(ffi_cif *cif __UNUSED__, void *result, void **args,
                     void *userdata __UNUSED__)
{
  mixed_struct *value = (mixed_struct *) args[9];

  CHECK(*(long *) args[0] == 1);
  CHECK(*(long *) args[1] == 2);
  CHECK(*(long *) args[2] == 3);
  CHECK(*(long *) args[3] == 4);
  CHECK(*(long *) args[4] == 5);
  CHECK(*(long *) args[5] == 6);
  CHECK(*(long *) args[6] == 7);
  CHECK(*(long *) args[7] == 8);
  CHECK(*(int *) args[8] == 9);
  CHECK(value->f1 == 1.5f);
  CHECK(value->f2 == 2.5f);
  CHECK(value->i == 17);
  CHECK(*(int *) args[10] == 11);

  *(ffi_arg *) result = (ffi_arg) struct_stack(1, 2, 3, 4, 5, 6, 7, 8,
                                               9, *value, 11);
}

typedef int (ABI_ATTR *struct_stack_fn)(long, long, long, long,
                                        long, long, long, long,
                                        int, mixed_struct, int);
#endif

int
main(void)
{
  ffi_cif cif;
  ffi_type mixed_type;
  ffi_type *mixed_elements[4];
  ffi_type *arg_types[12];
  void *args[11];
  long integers[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
  int before = 9;
  mixed_struct value = { 1.5f, 2.5f, 17 };
  int after = 11;
  ffi_arg result = 0;
  int expected;
  int i;

  mixed_type.size = 0;
  mixed_type.alignment = 0;
  mixed_type.type = FFI_TYPE_STRUCT;
  mixed_type.elements = mixed_elements;
  mixed_elements[0] = &ffi_type_float;
  mixed_elements[1] = &ffi_type_float;
  mixed_elements[2] = &ffi_type_sint;
  mixed_elements[3] = NULL;

  for (i = 0; i < 8; ++i)
    {
      arg_types[i] = &ffi_type_slong;
      args[i] = &integers[i];
    }
  arg_types[8] = &ffi_type_sint;
  args[8] = &before;
  arg_types[9] = &mixed_type;
  args[9] = &value;
  arg_types[10] = &ffi_type_sint;
  args[10] = &after;
  arg_types[11] = NULL;

  CHECK(ffi_prep_cif(&cif, ABI_NUM, 11, &ffi_type_sint, arg_types) == FFI_OK);

  expected = struct_stack(1, 2, 3, 4, 5, 6, 7, 8, before, value, after);
  ffi_call(&cif, FFI_FN(struct_stack), &result, args);
  if ((int) result != expected)
    {
      printf("native result: %d; ffi_call result: %d\n", expected, (int) result);
      fflush(stdout);
    }
  CHECK((int) result == expected);

#if FFI_CLOSURES
  {
    void *code;
    ffi_closure *closure = ffi_closure_alloc(sizeof(ffi_closure), &code);
    CHECK(closure != NULL);
    CHECK(ffi_prep_closure_loc(closure, &cif, struct_stack_closure,
                               NULL, code) == FFI_OK);
    CHECK(((struct_stack_fn) code)(1, 2, 3, 4, 5, 6, 7, 8,
                                   before, value, after) == expected);
    ffi_closure_free(closure);
  }
#endif

  return 0;
}

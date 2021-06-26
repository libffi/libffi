/* Area:       ffi_call, closure_call
   Purpose:    Single argument structs have a different ABI in emscripten.
   Limitations:        none.
   PR:         none.
   Originator: <hood@mit.edu>  */

/* { dg-do run } */
#include "ffitest.h"

typedef struct A {
  float a;
} A;

static struct A A_fn(float b0, struct A b1)
{
  b1.a += b0;
  return b1;
}

static void
A_gn(ffi_cif* cif __UNUSED__, void* resp, void** args,
     void* userdata __UNUSED__)
{
  float b0;
  struct A b1;

  b0 = *(float*)(args[0]);
  b1 = *(struct A*)(args[1]);

  *(A*)resp = A_fn(b0, b1);
}

int main (void)
{
  printf("123\n");
  ffi_cif cif;
  void *code;
  ffi_closure *pcl = ffi_closure_alloc(sizeof(ffi_closure), &code);
  void* args_dbl[3];
  ffi_type* cls_struct_fields[2];
  ffi_type cls_struct_type, cls_struct_type1;
  ffi_type* dbl_arg_types[3];

  float e_dbl = 12.125f;
  struct A f_dbl = { 31.625f };

  struct A res_dbl;

  cls_struct_type.size = 0;
  cls_struct_type.alignment = 0;
  cls_struct_type.type = FFI_TYPE_STRUCT;
  cls_struct_type.elements = cls_struct_fields;

  cls_struct_fields[0] = &ffi_type_float;
  cls_struct_fields[1] = NULL;

  dbl_arg_types[0] = &ffi_type_float;
  dbl_arg_types[1] = &cls_struct_type;
  dbl_arg_types[2] = NULL;

  res_dbl = A_fn(e_dbl, f_dbl);
  printf("0 res_dbl: %f \n", res_dbl.a);

  CHECK(ffi_prep_cif(&cif, FFI_DEFAULT_ABI, 2, &cls_struct_type,
                    dbl_arg_types) == FFI_OK);

  args_dbl[0] = &e_dbl;
  args_dbl[1] = &f_dbl;
  args_dbl[2] = NULL;


  ffi_call(&cif, FFI_FN(A_fn), &res_dbl, args_dbl);
  printf("1 res_dbl: %f \n", res_dbl.a);
  CHECK( res_dbl.a == (e_dbl + f_dbl.a));

  CHECK(ffi_prep_closure_loc(pcl, &cif, A_gn, NULL, code) == FFI_OK);

  res_dbl = ((A(*)(float, A))(code))(e_dbl, f_dbl);
  printf("2 res_dbl: %f \n", res_dbl.a);
  CHECK( res_dbl.a == (e_dbl + f_dbl.a));

  exit(0);
}

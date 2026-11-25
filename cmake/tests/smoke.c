#include <ffi.h>

#include <stdio.h>
#include <string.h>

static int
add_ints(int left, int right)
{
  return left + right;
}

static void
closure_add(ffi_cif *cif, void *result, void **arguments, void *user_data)
{
  int bias = *(const int *)user_data;
  (void)cif;
  *(int *)result = *(const int *)arguments[0]
                  + *(const int *)arguments[1] + bias;
}

int
main(void)
{
  ffi_cif cif;
  ffi_type *argument_types[] = { &ffi_type_sint, &ffi_type_sint };
  void *arguments[2];
  int left = 19;
  int right = 23;
  ffi_arg call_result = 0;
  int closure_result;
  int bias = 7;
  void *closure_code = NULL;
  ffi_closure *closure;

  if (strcmp(ffi_get_version(), FFI_VERSION_STRING) != 0
      || ffi_get_version_number() != FFI_VERSION_NUMBER)
    return 1;
  if (ffi_prep_cif(&cif, FFI_DEFAULT_ABI, 2, &ffi_type_sint,
                   argument_types) != FFI_OK)
    return 2;

  arguments[0] = &left;
  arguments[1] = &right;
  /* Integral results use ffi_arg-sized storage on the Win64 backend. */
  ffi_call(&cif, FFI_FN(add_ints), &call_result, arguments);
  if (call_result != 42)
    return 3;

  closure = (ffi_closure *)ffi_closure_alloc(sizeof(*closure), &closure_code);
  if (!closure)
    return 4;
  if (ffi_prep_closure_loc(closure, &cif, closure_add, &bias, closure_code)
      != FFI_OK)
    return 5;

  closure_result = ((int (*)(int, int))closure_code)(left, right);
  ffi_closure_free(closure);
  if (closure_result != 49)
    return 6;

  puts("libffi CMake smoke test passed");
  return 0;
}

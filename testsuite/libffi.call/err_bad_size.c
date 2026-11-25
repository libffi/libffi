/* Area:		ffi_prep_cif, ffi_prep_cif_var
   Purpose:		Refuse signatures whose sizes or argument counts do not
			fit libffi's fields, instead of truncating them.
   Limitations:	none.
   PR:			none.

   A struct whose member sizes add up past SIZE_MAX was laid out with a
   wrapped size; an argument list of 4 GiB or more wrapped the 32-bit
   cif->bytes; more than INT_MAX arguments overflowed int counters.  Each
   was accepted with FFI_OK and then overran the stack in ffi_call.  None
   of these checks allocates the memory such a call would need.  */

/* { dg-do run } */
#include "ffitest.h"

int
main (void)
{
  ffi_cif cif;
  ffi_type huge, wrapping, pair;
  ffi_type *huge_elements[2], *wrap_elements[3], *pair_elements[3];
  ffi_type *args[2];

  /* A struct of two members whose sizes add up past SIZE_MAX.  */
  huge.size = ((size_t) -1) / 2 + 1;
  huge.alignment = 1;
  huge.type = FFI_TYPE_STRUCT;
  huge_elements[0] = &ffi_type_uchar;
  huge_elements[1] = NULL;
  huge.elements = huge_elements;

  wrap_elements[0] = &huge;
  wrap_elements[1] = &huge;
  wrap_elements[2] = NULL;
  wrapping.size = 0;
  wrapping.alignment = 0;
  wrapping.type = FFI_TYPE_STRUCT;
  wrapping.elements = wrap_elements;

  args[0] = &wrapping;
  CHECK (ffi_prep_cif (&cif, FFI_DEFAULT_ABI, 1, &ffi_type_void, args)
	 == FFI_BAD_TYPEDEF);

  /* A single caller-sized struct of 1 GiB, as argument and as result.  */
  huge.size = (size_t) 1 << 30;
  args[0] = &huge;
  CHECK (ffi_prep_cif (&cif, FFI_DEFAULT_ABI, 1, &ffi_type_void, args)
	 == FFI_BAD_TYPEDEF);
  CHECK (ffi_prep_cif (&cif, FFI_DEFAULT_ABI, 0, &huge, NULL)
	 == FFI_BAD_TYPEDEF);

  /* Two arguments that are each acceptable but together too large.  */
  pair.size = (size_t) 160 << 20;
  pair.alignment = 1;
  pair.type = FFI_TYPE_STRUCT;
  pair_elements[0] = &ffi_type_uchar;
  pair_elements[1] = NULL;
  pair.elements = pair_elements;
  args[0] = args[1] = &pair;
  CHECK (ffi_prep_cif (&cif, FFI_DEFAULT_ABI, 1, &ffi_type_void, args)
	 == FFI_OK);
  CHECK (ffi_prep_cif (&cif, FFI_DEFAULT_ABI, 2, &ffi_type_void, args)
	 == FFI_BAD_TYPEDEF);

  /* More arguments than any port can count.  The count is refused before
     ARGS is read, so a short array is enough.  */
  args[0] = args[1] = &ffi_type_sint;
  CHECK (ffi_prep_cif (&cif, FFI_DEFAULT_ABI, 0x80000000u, &ffi_type_void,
		       args) == FFI_BAD_TYPEDEF);
  CHECK (ffi_prep_cif_var (&cif, FFI_DEFAULT_ABI, 1, 0x80000000u,
			   &ffi_type_void, args) == FFI_BAD_TYPEDEF);

  exit (0);
}

/* Area:	ffi_prep_cif, ffi_call
   Purpose:	A struct's layout must not depend on which other signatures
		were prepared before.
   Limitations:	none.
   PR:		none.

   On AIX the layout code stored the 8-byte alignment of a struct laid
   out as an outermost type in the shared ffi_type, and reused it when
   the same struct was later laid out as a non-first member.  Preparing a
   signature that takes struct inner by value therefore grew struct outer
   from 12 to 16 bytes, even for cifs prepared earlier, and ffi_call
   then copied more than it had reserved and misplaced later arguments.  */

/* { dg-do run } */
#include "ffitest.h"

struct inner { double d; };
struct outer { void *tag; struct inner in; };

static int ten_ok, outer_ok;
static int marker = 42;

static void
takes_ten (struct outer a0, struct outer a1, struct outer a2,
	   struct outer a3, struct outer a4, struct outer a5,
	   struct outer a6, struct outer a7, struct outer a8,
	   struct outer a9)
{
  struct outer a[10];
  int i;

  a[0] = a0; a[1] = a1; a[2] = a2; a[3] = a3; a[4] = a4;
  a[5] = a5; a[6] = a6; a[7] = a7; a[8] = a8; a[9] = a9;
  ten_ok = 1;
  for (i = 0; i < 10; i++)
    if (a[i].tag != (void *) (uintptr_t) (i + 1) || a[i].in.d != i + 0.5)
      ten_ok = 0;
}

static void
takes_inner (struct inner in)
{
  (void) in;
}

static void
takes_outer (struct outer o, int *p)
{
  outer_ok = o.tag == (void *) (uintptr_t) 1 && o.in.d == 0.5 && p == &marker;
}

int
main (void)
{
  ffi_type inner_type, outer_type;
  ffi_type *inner_elements[2], *outer_elements[3];
  ffi_type *ten_args[10], *inner_args[1], *outer_args[2];
  ffi_cif ten_cif, inner_cif, outer_cif;
  struct outer values[10];
  void *ten_values[10], *outer_values[2];
  int *p = &marker;
  int i;

  inner_elements[0] = &ffi_type_double;
  inner_elements[1] = NULL;
  inner_type.size = 0;
  inner_type.alignment = 0;
  inner_type.type = FFI_TYPE_STRUCT;
  inner_type.elements = inner_elements;

  outer_elements[0] = &ffi_type_pointer;
  outer_elements[1] = &inner_type;
  outer_elements[2] = NULL;
  outer_type.size = 0;
  outer_type.alignment = 0;
  outer_type.type = FFI_TYPE_STRUCT;
  outer_type.elements = outer_elements;

  for (i = 0; i < 10; i++)
    {
      ten_args[i] = &outer_type;
      values[i].tag = (void *) (uintptr_t) (i + 1);
      values[i].in.d = i + 0.5;
      ten_values[i] = &values[i];
    }
  inner_args[0] = &inner_type;
  outer_args[0] = &outer_type;
  outer_args[1] = &ffi_type_pointer;
  outer_values[0] = &values[0];
  outer_values[1] = &p;

  CHECK (ffi_prep_cif (&ten_cif, FFI_DEFAULT_ABI, 10, &ffi_type_void,
		       ten_args) == FFI_OK);
  CHECK (outer_type.size == sizeof (struct outer));
  CHECK (ffi_prep_cif (&inner_cif, FFI_DEFAULT_ABI, 1, &ffi_type_void,
		       inner_args) == FFI_OK);
  CHECK (ffi_prep_cif (&outer_cif, FFI_DEFAULT_ABI, 2, &ffi_type_void,
		       outer_args) == FFI_OK);
  CHECK (outer_type.size == sizeof (struct outer));
  CHECK (inner_type.size == sizeof (struct inner));

  ffi_call (&outer_cif, FFI_FN (takes_outer), NULL, outer_values);
  CHECK (outer_ok);
  ffi_call (&ten_cif, FFI_FN (takes_ten), NULL, ten_values);
  CHECK (ten_ok);
  (void) takes_inner;

  exit (0);
}

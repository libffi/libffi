/* Area:	ffi_prep_cif
   Purpose:	Validate that malformed vector type descriptors are rejected
		with FFI_BAD_TYPEDEF, and that a well-formed vector is accepted
		with the computed power-of-two size and min(size,16) alignment.
   Limitations:	none.
   PR:		none.
   Originator:	libffi vector support.  */

/* { dg-do run } */

#include "vector.h"

int
main (void)
{
  ffi_cif cif;

  /* Heterogeneous lanes (float mixed with double) -> FFI_BAD_TYPEDEF.  */
  {
    ffi_type vt;
    ffi_type *elems[3];
    ffi_type *args[1];
    elems[0] = &ffi_type_float;
    elems[1] = &ffi_type_double;
    elems[2] = NULL;
    vt.size = 0;
    vt.alignment = 0;
    vt.type = FFI_TYPE_VECTOR;
    vt.elements = elems;
    args[0] = &vt;
    CHECK (ffi_prep_cif (&cif, FFI_DEFAULT_ABI, 1, &ffi_type_void, args)
	   == FFI_BAD_TYPEDEF);
  }

  /* An empty (zero-lane) vector -> FFI_BAD_TYPEDEF.  */
  {
    ffi_type vt;
    ffi_type *elems[1];
    ffi_type *args[1];
    elems[0] = NULL;
    vt.size = 0;
    vt.alignment = 0;
    vt.type = FFI_TYPE_VECTOR;
    vt.elements = elems;
    args[0] = &vt;
    CHECK (ffi_prep_cif (&cif, FFI_DEFAULT_ABI, 1, &ffi_type_void, args)
	   == FFI_BAD_TYPEDEF);
  }

  /* A non-scalar (struct) lane type -> FFI_BAD_TYPEDEF.  */
  {
    ffi_type inner;
    ffi_type *inner_elems[2];
    ffi_type vt;
    ffi_type *elems[3];
    ffi_type *args[1];
    inner_elems[0] = &ffi_type_float;
    inner_elems[1] = NULL;
    inner.size = 0;
    inner.alignment = 0;
    inner.type = FFI_TYPE_STRUCT;
    inner.elements = inner_elems;
    elems[0] = &inner;
    elems[1] = &inner;
    elems[2] = NULL;
    vt.size = 0;
    vt.alignment = 0;
    vt.type = FFI_TYPE_VECTOR;
    vt.elements = elems;
    args[0] = &vt;
    CHECK (ffi_prep_cif (&cif, FFI_DEFAULT_ABI, 1, &ffi_type_void, args)
	   == FFI_BAD_TYPEDEF);
  }

  /* A well-formed 3 x float vector is accepted with computed layout.  */
  {
    ffi_type vt;
    ffi_type *elems[4];
    ffi_type *args[1];
    make_vector_type (&vt, elems, &ffi_type_float, 3);
    args[0] = &vt;
    CHECK (ffi_prep_cif (&cif, FFI_DEFAULT_ABI, 1, &ffi_type_void, args)
	   == FFI_OK);
    CHECK (vt.size == 16);         /* 12 rounded up to 16 */
    CHECK (vt.alignment == 16);    /* min(16, 16) */
  }

  /* An 8-byte vector gets alignment 8 = min(8, 16).  */
  {
    ffi_type vt;
    ffi_type *elems[3];
    ffi_type *args[1];
    make_vector_type (&vt, elems, &ffi_type_float, 2);
    args[0] = &vt;
    CHECK (ffi_prep_cif (&cif, FFI_DEFAULT_ABI, 1, &ffi_type_void, args)
	   == FFI_OK);
    CHECK (vt.size == 8);
    CHECK (vt.alignment == 8);
  }

#if defined(__x86_64__) && !defined(__ILP32__)
  /* The Win64 backend (FFI_WIN64 / FFI_GNUW64) built into the x86-64 library
     does not marshal vectors: a well-formed vector must be rejected as a
     return type, as an argument, and nested inside a struct, whatever its
     size.  A 64-byte vector return used to be accepted and then overflowed
     the closure's 16-byte result slot.  */
  {
    static const ffi_abi abis[] = { FFI_WIN64, FFI_GNUW64 };
    unsigned i;
    for (i = 0; i < sizeof abis / sizeof abis[0]; i++)
      {
	ffi_type vt, st;
	ffi_type *elems[9];
	ffi_type *st_elems[3];
	ffi_type *args[1];

	make_vector_type (&vt, elems, &ffi_type_uint64, 8);
	CHECK (ffi_prep_cif (&cif, abis[i], 0, &vt, NULL) == FFI_BAD_TYPEDEF);

	make_vector_type (&vt, elems, &ffi_type_float, 4);
	CHECK (ffi_prep_cif (&cif, abis[i], 0, &vt, NULL) == FFI_BAD_TYPEDEF);
	args[0] = &vt;
	CHECK (ffi_prep_cif (&cif, abis[i], 1, &ffi_type_void, args)
	       == FFI_BAD_TYPEDEF);

	st_elems[0] = &ffi_type_double;
	st_elems[1] = &vt;
	st_elems[2] = NULL;
	st.size = 0;
	st.alignment = 0;
	st.type = FFI_TYPE_STRUCT;
	st.elements = st_elems;
	args[0] = &st;
	CHECK (ffi_prep_cif (&cif, abis[i], 1, &ffi_type_void, args)
	       == FFI_BAD_TYPEDEF);
	CHECK (ffi_prep_cif (&cif, abis[i], 0, &st, NULL) == FFI_BAD_TYPEDEF);
      }
  }
#endif

  printf ("vector validation ok\n");
  exit (0);
}

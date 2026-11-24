/* Area:		ffi_prep_cif
   Purpose:		Ports that do not marshal vectors must reject any
			signature that mentions one, directly or nested in a
			struct, with FFI_BAD_TYPEDEF.  CRIS overrides
			ffi_prep_cif_core and once missed this check, so a
			closure's vector result overflowed its 8-byte return
			slot.
   Limitations:	Passes trivially on ports that define
			FFI_TARGET_HAS_VECTOR_TYPE; libffi.vector covers those.
   PR:			none.
   Originator:	libffi vector support.  */

/* { dg-do run } */

#include "ffitest.h"

int main (void)
{
#ifndef FFI_TARGET_HAS_VECTOR_TYPE
	ffi_cif cif;
	ffi_type vt, st;
	ffi_type *lanes[5];
	ffi_type *st_elems[3];
	ffi_type *args[1];
	int i;

	for (i = 0; i < 4; i++)
		lanes[i] = &ffi_type_float;
	lanes[4] = NULL;
	vt.size = 0;
	vt.alignment = 0;
	vt.type = FFI_TYPE_VECTOR;
	vt.elements = lanes;

	CHECK(ffi_prep_cif(&cif, FFI_DEFAULT_ABI, 0, &vt, NULL)
	      == FFI_BAD_TYPEDEF);

	args[0] = &vt;
	CHECK(ffi_prep_cif(&cif, FFI_DEFAULT_ABI, 1, &ffi_type_void, args)
	      == FFI_BAD_TYPEDEF);

	st_elems[0] = &ffi_type_double;
	st_elems[1] = &vt;
	st_elems[2] = NULL;
	st.size = 0;
	st.alignment = 0;
	st.type = FFI_TYPE_STRUCT;
	st.elements = st_elems;
	CHECK(ffi_prep_cif(&cif, FFI_DEFAULT_ABI, 0, &st, NULL)
	      == FFI_BAD_TYPEDEF);
	args[0] = &st;
	CHECK(ffi_prep_cif(&cif, FFI_DEFAULT_ABI, 1, &ffi_type_void, args)
	      == FFI_BAD_TYPEDEF);
#endif

	exit(0);
}

/* Area:	ffi_prep_raw_closure_loc
   Purpose:	Check a raw-API closure that returns a struct in memory.
		The i386 raw closure entry points gave the handler a 16-byte
		temporary as RET and ignored the hidden return pointer, so a
		handler storing a larger struct overwrote the saved %ebx and
		the return address.  Such cifs are now refused there; where a
		port accepts one, the struct must come back intact.
   Limitations:	none.
   PR:		none.  */

/* { dg-do run } */
#include "ffitest.h"

#if FFI_CLOSURES && !FFI_NO_RAW_API

typedef struct
{
  unsigned int field[12];
} twelve;

typedef twelve (*twelve_fn) (int);

static void
raw_handler (ffi_cif *cif, void *ret, ffi_raw *args, void *ud __UNUSED__)
{
  twelve result;
  int i;

  for (i = 0; i < 12; i++)
    result.field[i] = args[0].sint * 100 + i;
  memcpy (ret, &result, cif->rtype->size);
}

int
main (void)
{
  ffi_cif cif;
  ffi_type *args[1] = { &ffi_type_sint };
  ffi_type twelve_type, *elements[13];
  ffi_raw_closure *cl;
  ffi_status status;
  void *code;
  twelve r;
  int i;

  for (i = 0; i < 12; i++)
    elements[i] = &ffi_type_uint;
  elements[12] = NULL;
  twelve_type.size = 0;
  twelve_type.alignment = 0;
  twelve_type.type = FFI_TYPE_STRUCT;
  twelve_type.elements = elements;

  CHECK (ffi_prep_cif (&cif, FFI_DEFAULT_ABI, 1, &twelve_type, args)
	 == FFI_OK);

  cl = ffi_closure_alloc (sizeof (ffi_raw_closure), &code);
  CHECK (cl != NULL);
  status = ffi_prep_raw_closure_loc (cl, &cif, raw_handler, NULL, code);
  CHECK (status == FFI_OK || status == FFI_BAD_TYPEDEF);

  if (status == FFI_OK)
    {
      r = ((twelve_fn) code) (7);
      for (i = 0; i < 12; i++)
	CHECK (r.field[i] == 700u + i);
    }

  ffi_closure_free (cl);
  printf ("raw struct closure ok\n");
  exit (0);
}

#else
int main (void) { return 0; }
#endif

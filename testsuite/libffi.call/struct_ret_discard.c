/* Area:	ffi_call
   Purpose:	Discard a large struct result by passing a NULL rvalue.
   Limitations:	none.
   PR:		none.

   The manual allows rvalue == NULL for any return type, with libffi
   supplying scratch space for a struct returned in memory.  On SPARC64
   that scratch pointer was 176 bytes too high, so the callee's result
   landed on ffi_call_int's saved frame pointer and return address, and
   ffi_call returned to an address taken from the discarded struct once
   its register window had been spilled; the callee recurses so that it
   is.  */

/* { dg-do run } */
#include "ffitest.h"

typedef struct
{
  unsigned long w[16];
} big;

static int calls;

/* Deep enough to spill every register window of the callers.  */
static unsigned long __attribute__ ((noinline))
recurse (int depth)
{
  volatile unsigned long local = depth;

  return depth > 0 ? recurse (depth - 1) + local : local;
}

static big
make_big (unsigned long seed)
{
  big b;
  int i;

  calls++;
  CHECK (recurse (64) == 64 * 65 / 2);
  for (i = 0; i < 16; i++)
    b.w[i] = seed + i;
  return b;
}

int
main (void)
{
  ffi_cif cif;
  ffi_type big_type, *elements[17], *args[1] = { &ffi_type_ulong };
  unsigned long seed = 0x4141414141414140UL & (unsigned long) -1;
  void *values[1] = { &seed };
  big result;
  int i;

  for (i = 0; i < 16; i++)
    elements[i] = &ffi_type_ulong;
  elements[16] = NULL;
  big_type.size = 0;
  big_type.alignment = 0;
  big_type.type = FFI_TYPE_STRUCT;
  big_type.elements = elements;

  CHECK (ffi_prep_cif (&cif, FFI_DEFAULT_ABI, 1, &big_type, args) == FFI_OK);

  /* The callee writes the whole struct; ffi_call must come back.  */
  ffi_call (&cif, FFI_FN (make_big), NULL, values);
  ffi_call (&cif, FFI_FN (make_big), NULL, values);
  CHECK (calls == 2);

  /* And the same cif still returns into a real buffer.  */
  ffi_call (&cif, FFI_FN (make_big), &result, values);
  CHECK (calls == 3);
  for (i = 0; i < 16; i++)
    CHECK (result.w[i] == seed + i);

  exit (0);
}

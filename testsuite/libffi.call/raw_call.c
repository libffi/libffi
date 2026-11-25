/* Area:	ffi_raw_call
   Purpose:	Check that ffi_raw_call delivers all stack arguments intact.
   Limitations:	none.
   PR:		none.
   Originator:	i386 ffi_raw_call alloca alignment regression.

   Regression test: the i386 ffi_raw_call masked its alloca'd argument
   block with ~16 instead of rounding it up to 16, so for half of all
   stack positions the block began 16 bytes below the stack pointer and
   the call into ffi_call_i386 pushed its return address over the fourth
   argument word.  Whether a call was hit depended on bit 4 of the stack
   pointer, so the call is made from two frames 16 bytes apart.  */

/* { dg-do run } */

#include "ffitest.h"

/* The raw API can be configured out (e.g. Emscripten builds with
   --disable-raw-api); match raw_api.c's own guard so the test links.  */
#if !FFI_NO_RAW_API

#ifdef _MSC_VER
#include <malloc.h>
#define ALLOCA _alloca
#else
#define ALLOCA __builtin_alloca
#endif

#define NARGS 8

static int ABI_ATTR
sum8 (int a, int b, int c, int d, int e, int f, int g, int h)
{
  CHECK (a == 1);
  CHECK (b == 2);
  CHECK (c == 3);
  CHECK (d == 4);
  CHECK (e == 5);
  CHECK (f == 6);
  CHECK (g == 7);
  CHECK (h == 8);
  return a + b + c + d + e + f + g + h;
}

#ifdef __GNUC__
__attribute__((noinline))
#endif
static void
call_from_depth (ffi_cif *cif, size_t depth)
{
  volatile char *pad = ALLOCA (depth);
  int values[NARGS];
  void *args[NARGS];
  ffi_raw *raw = ALLOCA (ffi_raw_size (cif));
  ffi_arg rint;
  int i;

  pad[0] = 0;

  for (i = 0; i < NARGS; i++)
    {
      values[i] = i + 1;
      args[i] = &values[i];
    }

  ffi_ptrarray_to_raw (cif, args, raw);
  rint = 0;
  ffi_raw_call (cif, FFI_FN (sum8), &rint, raw);
  CHECK ((int) rint == 36);
}

int
main (void)
{
  ffi_cif cif;
  ffi_type *arg_types[NARGS];
  int i;

  for (i = 0; i < NARGS; i++)
    arg_types[i] = &ffi_type_sint;

  CHECK (ffi_prep_cif (&cif, ABI_NUM, NARGS, &ffi_type_sint,
		       arg_types) == FFI_OK);

  call_from_depth (&cif, 16);
  call_from_depth (&cif, 32);

  exit (0);
}

#else
int main (void) { return 0; }
#endif

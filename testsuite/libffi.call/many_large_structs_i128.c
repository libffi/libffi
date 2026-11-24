/* Area:	ffi_call
   Purpose:	Pass large by-value structs interleaved with __int128 on AArch64.
   Limitations:	none.
   PR:		none.
   Originator:	AArch64 large-struct stack accounting regression.

   Regression test: on AArch64, composites larger than 16 bytes are passed
   by invisible reference, and once X0-X7 are exhausted the 8-byte pointer
   to the copy is spilled to the stack.  That pointer leaves the NSAA at an
   odd doubleword, so a following __int128 needs 8 bytes of padding that
   the cif->bytes budget did not charge.  With enough (struct, __int128)
   pairs on the stack the payload copies (growing down) ran into the
   spilled pointers (growing up), and the callee read a by-value argument
   through an address made of another argument's bytes.  */

/* { dg-do run } */
#include "ffitest.h"

#if defined(FFI_TARGET_HAS_INT128) && defined(__SIZEOF_INT128__)

typedef __int128_t i128;

typedef struct { i128 key, value; } record;

#define NPAIRS 13
#define NARGS (2 * NPAIRS + 1)

#define KEY(n)   (((i128) (n) << 64) | (0x1000 + (n)))
#define VALUE(n) (((i128) (n) << 64) | (0x2000 + (n)))
#define INT(n)   (((i128) (n) << 64) | (0x3000 + (n)))

#define R(n) record r##n
#define I(n) i128 i##n

/* Return the number of arguments that did not arrive intact.  */
static int ABI_ATTR
count_bad (R(0), I(0), R(1), I(1), R(2), I(2), R(3), I(3), R(4), I(4),
	   R(5), I(5), R(6), I(6), R(7), I(7), R(8), I(8), R(9), I(9),
	   R(10), I(10), R(11), I(11), R(12), I(12), R(13))
{
  record *r[NPAIRS + 1];
  i128 v[NPAIRS];
  int n, bad = 0;

  r[0] = &r0;   r[1] = &r1;   r[2] = &r2;   r[3] = &r3;   r[4] = &r4;
  r[5] = &r5;   r[6] = &r6;   r[7] = &r7;   r[8] = &r8;   r[9] = &r9;
  r[10] = &r10; r[11] = &r11; r[12] = &r12; r[13] = &r13;
  v[0] = i0;   v[1] = i1;   v[2] = i2;   v[3] = i3;   v[4] = i4;
  v[5] = i5;   v[6] = i6;   v[7] = i7;   v[8] = i8;   v[9] = i9;
  v[10] = i10; v[11] = i11; v[12] = i12;

  for (n = 0; n <= NPAIRS; n++)
    if (r[n]->key != KEY (n) || r[n]->value != VALUE (n))
      bad++;
  for (n = 0; n < NPAIRS; n++)
    if (v[n] != INT (n))
      bad++;

  return bad;
}

int main (void)
{
  ffi_cif cif;
  ffi_type *args[NARGS];
  void *values[NARGS];
  ffi_type record_type;
  ffi_type *record_elements[3];
  record in_r[NPAIRS + 1];
  i128 in_i[NPAIRS];
  ffi_arg result = 1;
  int n;

  record_type.size = 0;
  record_type.alignment = 0;
  record_type.type = FFI_TYPE_STRUCT;
  record_elements[0] = &ffi_type_sint128;
  record_elements[1] = &ffi_type_sint128;
  record_elements[2] = NULL;
  record_type.elements = record_elements;

  for (n = 0; n <= NPAIRS; n++)
    {
      in_r[n].key = KEY (n);
      in_r[n].value = VALUE (n);
      args[2 * n] = &record_type;
      values[2 * n] = &in_r[n];
      if (n < NPAIRS)
	{
	  in_i[n] = INT (n);
	  args[2 * n + 1] = &ffi_type_sint128;
	  values[2 * n + 1] = &in_i[n];
	}
    }

  CHECK(ffi_prep_cif(&cif, ABI_NUM, NARGS, &ffi_type_sint, args) == FFI_OK);

  ffi_call(&cif, FFI_FN(count_bad), &result, values);

  CHECK((int) result == 0);

  exit(0);
}

#else
int main (void) { return 0; }
#endif

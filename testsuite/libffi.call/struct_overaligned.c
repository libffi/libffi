/* Area:	ffi_call
   Purpose:	Pass a by-value struct aligned to more than the stack.
   Limitations:	none.
   PR:		none.
   Originator:	over-aligned stack argument regression.

   Regression test: ffi_prep_cif sizes the stack argument area by aligning
   offsets into it, while ffi_call placed each in-memory argument by
   aligning its address inside an area that is only as aligned as the
   stack.  For an argument aligned to more than that, ffi_call inserted
   padding that had not been reserved, copied the argument past the end of
   the area over its own frame, and left the callee looking for the
   argument in the wrong place.  On AArch64, where both structs below go
   by reference, the copy of a large struct after an over-aligned one was
   placed below the area instead (the second call).  How much padding was inserted
   depended on where the frame happened to lie, so the call is made at
   every stack position modulo the alignment.  A 128-byte-aligned struct
   passed between other stack arguments must arrive intact at all of
   them.  */

/* { dg-do run } */
#include "ffitest.h"

/* MSVC lacks the aligned attribute used here and refuses over-aligned
   by-value parameters (C2719).  Not every target has <alloca.h> (MinGW
   does not), so use the builtin.  */
#ifndef _MSC_VER

#define BLOCK_ALIGN 128
#define BLOCK_WORDS 16

typedef struct
{
  unsigned long w[BLOCK_WORDS];
} __attribute__ ((aligned (BLOCK_ALIGN))) block;

/* Larger than 16 bytes and not floating point, so AArch64 passes it by
   reference, through a copy placed after the block's.  */
typedef struct
{
  unsigned long a, b, c;
} trio;

/* Enough register arguments to fill the argument registers of most ports,
   then the rest on the stack.  Every argument has its own weight, so one
   read from the wrong place changes the result.  */
static unsigned long
weigh (unsigned long r1, unsigned long r2, unsigned long r3,
       unsigned long r4, unsigned long r5, unsigned long r6,
       unsigned long r7, unsigned long r8,
       unsigned long before, block b, trio t, unsigned long after)
{
  unsigned long sum = r1 + r2 * 2 + r3 * 3 + r4 * 5 + r5 * 7 + r6 * 11
		      + r7 * 13 + r8 * 17;
  int i;

  sum += before * 19;
  for (i = 0; i < BLOCK_WORDS; i++)
    sum += b.w[i] * (i + 23);
  sum += t.a * 41 + t.b * 43 + t.c * 47;
  sum += after * 53;

  return sum;
}

/* The over-aligned struct first, then a large one.  */
static unsigned long
pair (block b, trio t)
{
  unsigned long sum = 0;
  int i;

  for (i = 0; i < BLOCK_WORDS; i++)
    sum += b.w[i] * (i + 3);
  sum += t.a * 41 + t.b * 43 + t.c * 47;

  return sum;
}

static ffi_cif cif, pair_cif;
static block in;
static trio three;
static unsigned long words[10];
static void *values[12];
static void *pair_values[2];

/* Make the call with 8 * (DEPTH + 1) more bytes of stack in use.  */
static unsigned long __attribute__ ((noinline))
call_at_depth (int depth)
{
  volatile char *pad = __builtin_alloca (8 * (depth + 1));
  ffi_arg result = 0;

  pad[0] = 0;
  ffi_call(&cif, FFI_FN(weigh), &result, values);

  return (unsigned long) result;
}

static unsigned long __attribute__ ((noinline))
call_pair_at_depth (int depth)
{
  volatile char *pad = __builtin_alloca (8 * (depth + 1));
  ffi_arg result = 0;

  pad[0] = 0;
  ffi_call(&pair_cif, FFI_FN(pair), &result, pair_values);

  return (unsigned long) result;
}

int main (void)
{
  ffi_type *args[12], *pair_args[2];
  ffi_type block_type, trio_type;
  ffi_type *block_elements[BLOCK_WORDS + 1];
  ffi_type *trio_elements[4];
  unsigned long expected;
  int i;

  /* An over-aligned struct cannot be laid out by libffi, so its size and
     alignment are given, as for a union.  */
  block_type.size = sizeof (block);
  block_type.alignment = __alignof__ (block);
  block_type.type = FFI_TYPE_STRUCT;
  for (i = 0; i < BLOCK_WORDS; i++)
    block_elements[i] = &ffi_type_ulong;
  block_elements[BLOCK_WORDS] = NULL;
  block_type.elements = block_elements;

  trio_type.size = 0;
  trio_type.alignment = 0;
  trio_type.type = FFI_TYPE_STRUCT;
  trio_elements[0] = trio_elements[1] = trio_elements[2] = &ffi_type_ulong;
  trio_elements[3] = NULL;
  trio_type.elements = trio_elements;

  for (i = 0; i < 10; i++)
    words[i] = 0x1111UL * (i + 1);
  for (i = 0; i < BLOCK_WORDS; i++)
    in.w[i] = 0x10001UL * (i + 1);
  three.a = 0x70007UL;
  three.b = 0x80008UL;
  three.c = 0x90009UL;

  for (i = 0; i < 9; i++)
    {
      args[i] = &ffi_type_ulong;
      values[i] = &words[i];
    }
  args[9] = &block_type;
  values[9] = &in;
  args[10] = &trio_type;
  values[10] = &three;
  args[11] = &ffi_type_ulong;
  values[11] = &words[9];

  expected = weigh (words[0], words[1], words[2], words[3], words[4],
		    words[5], words[6], words[7], words[8], in, three,
		    words[9]);

  CHECK(ffi_prep_cif(&cif, FFI_DEFAULT_ABI, 12, &ffi_type_ulong, args) == FFI_OK);

  for (i = 0; i < BLOCK_ALIGN / 8; i++)
    CHECK(call_at_depth (i) == expected);

  pair_args[0] = &block_type;
  pair_values[0] = &in;
  pair_args[1] = &trio_type;
  pair_values[1] = &three;
  expected = pair (in, three);

  CHECK(ffi_prep_cif(&pair_cif, FFI_DEFAULT_ABI, 2, &ffi_type_ulong, pair_args) == FFI_OK);

  for (i = 0; i < BLOCK_ALIGN / 8; i++)
    CHECK(call_pair_at_depth (i) == expected);

  exit(0);
}

#else
int main (void) { return 0; }
#endif

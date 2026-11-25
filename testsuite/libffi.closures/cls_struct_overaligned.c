/* Area:	closure_call
   Purpose:	Receive a by-value struct aligned to more than the stack.
   Limitations:	none.
   PR:		none.
   Originator:	over-aligned stack argument regression.

   The closure must find each stack argument where a compiled caller put
   it, whatever the stack position of the call, so the closure is called
   at every stack position modulo the struct's alignment.  */

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

typedef struct
{
  unsigned long a, b, c;
} trio;

typedef unsigned long (*weigh_fn) (unsigned long, unsigned long,
				   unsigned long, unsigned long,
				   unsigned long, unsigned long,
				   unsigned long, unsigned long,
				   unsigned long, block, trio,
				   unsigned long);

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

static void
weigh_gn (ffi_cif *cif __UNUSED__, void *resp, void **args,
	  void *userdata __UNUSED__)
{
  unsigned long *w[9];
  int i;

  for (i = 0; i < 9; i++)
    w[i] = (unsigned long *) args[i];
  *(ffi_arg *) resp = weigh (*w[0], *w[1], *w[2], *w[3], *w[4], *w[5],
			     *w[6], *w[7], *w[8], *(block *) args[9],
			     *(trio *) args[10], *(unsigned long *) args[11]);
}

static block in;
static trio three;
static unsigned long words[10];

/* Make the call with 8 * (DEPTH + 1) more bytes of stack in use.  */
static unsigned long __attribute__ ((noinline))
call_at_depth (weigh_fn fn, int depth)
{
  volatile char *pad = __builtin_alloca (8 * (depth + 1));

  pad[0] = 0;
  return fn (words[0], words[1], words[2], words[3], words[4], words[5],
	     words[6], words[7], words[8], in, three, words[9]);
}

int main (void)
{
  ffi_cif cif;
  void *code;
  ffi_closure *pcl = ffi_closure_alloc (sizeof (ffi_closure), &code);
  ffi_type *args[12];
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
    args[i] = &ffi_type_ulong;
  args[9] = &block_type;
  args[10] = &trio_type;
  args[11] = &ffi_type_ulong;

  expected = weigh (words[0], words[1], words[2], words[3], words[4],
		    words[5], words[6], words[7], words[8], in, three,
		    words[9]);

  CHECK(ffi_prep_cif(&cif, FFI_DEFAULT_ABI, 12, &ffi_type_ulong, args) == FFI_OK);
  CHECK(ffi_prep_closure_loc(pcl, &cif, weigh_gn, NULL, code) == FFI_OK);

  for (i = 0; i < BLOCK_ALIGN / 8; i++)
    CHECK(call_at_depth ((weigh_fn) code, i) == expected);

  ffi_closure_free (pcl);
  exit(0);
}

#else
int main (void) { return 0; }
#endif

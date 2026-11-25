/* Area:	closure_call
   Purpose:	Call a closure taking structs of floats from a caller that
		passes every argument in registers.
   Limitations:	none.
   PR:		none.
   Originator:	powerpc64 ELFv2 parameter save area regression.

   On powerpc64 ELFv2 the caller allocates a parameter save area only if
   some argument is passed in memory.  Here 14 floats need more than the 13
   FPRs, but the arguments take eight doublewords, so the excess floats go
   in a GPR and nothing is in memory.  libffi assumed a save area anyway,
   and the closure entry stored r3-r10 over the caller's frame, replacing
   the saved return address of call_it.  */

/* { dg-do run } */
#include "ffitest.h"

typedef struct { float f[2]; } float2;
typedef struct { float f[4]; } float4;
typedef struct { float f[8]; } float8;

typedef unsigned long (*callback) (float4, unsigned long, float8, float2);

static void
callback_gn (ffi_cif *cif __UNUSED__, void *resp, void **args,
	     void *userdata __UNUSED__)
{
  float4 *p = (float4 *) args[0];
  unsigned long id = *(unsigned long *) args[1];
  float8 *w = (float8 *) args[2];
  float2 *uv = (float2 *) args[3];
  float sum = 0;
  int i;

  for (i = 0; i < 4; i++)
    sum += p->f[i] * (i + 1);
  for (i = 0; i < 8; i++)
    sum += w->f[i] * (i + 5);
  sum += uv->f[0] * 13 + uv->f[1] * 14;

  *(ffi_arg *) resp = id * 10000 + (unsigned long) sum;
}

static float4 pos = { { 1, 2, 3, 4 } };
static float8 weights = { { 5, 6, 7, 8, 9, 10, 11, 12 } };
static float2 uv = { { 13, 14 } };

/* Makes no other call, so it has no parameter save area of its own; the
   addition keeps the call from becoming a tail call.  */
static unsigned long __attribute__ ((noinline))
call_it (callback fn)
{
  return fn (pos, 0x1234, weights, uv) + 1;
}

int main (void)
{
  ffi_cif cif;
  void *code;
  ffi_closure *pcl = ffi_closure_alloc (sizeof (ffi_closure), &code);
  ffi_type float2_type, float4_type, float8_type;
  ffi_type *e2[3], *e4[5], *e8[9], *args[4];
  unsigned long expected;
  int i;

  for (i = 0; i < 8; i++)
    e8[i] = &ffi_type_float;
  e8[8] = NULL;
  for (i = 0; i < 4; i++)
    e4[i] = &ffi_type_float;
  e4[4] = NULL;
  e2[0] = e2[1] = &ffi_type_float;
  e2[2] = NULL;
  float2_type.size = float4_type.size = float8_type.size = 0;
  float2_type.alignment = float4_type.alignment = float8_type.alignment = 0;
  float2_type.type = float4_type.type = float8_type.type = FFI_TYPE_STRUCT;
  float2_type.elements = e2;
  float4_type.elements = e4;
  float8_type.elements = e8;

  args[0] = &float4_type;
  args[1] = &ffi_type_ulong;
  args[2] = &float8_type;
  args[3] = &float2_type;

  /* 1*1 + 2*2 + 3*3 + 4*4 + (5..12)*(5..12) + 13*13 + 14*14  */
  expected = 0x1234UL * 10000 + 30 + 620 + 365 + 1;

  CHECK(ffi_prep_cif(&cif, FFI_DEFAULT_ABI, 4, &ffi_type_ulong, args) == FFI_OK);
  CHECK(ffi_prep_closure_loc(pcl, &cif, callback_gn, NULL, code) == FFI_OK);

  CHECK(call_it ((callback) code) == expected);

  ffi_closure_free (pcl);
  exit(0);
}

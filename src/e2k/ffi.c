/* -----------------------------------------------------------------------
   ffi.c - Copyright 2021-2022 Ilya Kurdyukov <jpegqs@gmail.com> for BaseALT, Ltd

   Elbrus 2000 Foreign Function Interface

   Permission is hereby granted, free of charge, to any person obtaining
   a copy of this software and associated documentation files (the
   ``Software''), to deal in the Software without restriction, including
   without limitation the rights to use, copy, modify, merge, publish,
   distribute, sublicense, and/or sell copies of the Software, and to
   permit persons to whom the Software is furnished to do so, subject to
   the following conditions:

   The above copyright notice and this permission notice shall be included
   in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED ``AS IS'', WITHOUT WARRANTY OF ANY KIND,
   EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
   NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
   HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
   WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
   DEALINGS IN THE SOFTWARE.
   ----------------------------------------------------------------------- */

#include <ffi.h>
#include <ffi_common.h>

#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define ALIGN(a, b) (((a) + (b) - 1) & -(b))

#define ARG_REGS 8
#define ALLOCA_EXTRA 4
typedef struct { long r[ARG_REGS]; } regs_t;

/* Perform machine dependent cif processing */
ffi_status
ffi_prep_cif_machdep(ffi_cif *cif)
{
  long bytes = 0, i, size, align, nargs = cif->nargs;
  ffi_type **arg_types = cif->arg_types;

  for (i = 0; i < nargs; i++)
    {
      size = arg_types[i]->size;
      align = size > 8 ? 16 : 8;
      bytes = ALIGN(bytes, align) + size;
    }
  cif->bytes = ALIGN(bytes, 8);

  size = cif->rtype->size;
  bytes = MAX(bytes, size);
  bytes = MAX(bytes, sizeof(regs_t));
  bytes = ALIGN(bytes, 16) - ALLOCA_EXTRA * sizeof(long);

  switch (cif->rtype->type)
    {
    case FFI_TYPE_INT:
    case FFI_TYPE_SINT8:
    case FFI_TYPE_UINT8:
    case FFI_TYPE_SINT16:
    case FFI_TYPE_UINT16:
    case FFI_TYPE_SINT32:
    case FFI_TYPE_UINT32:
      bytes |= 1;
    }

  cif->flags = bytes;

  return FFI_OK;
}

#define MAKE_CALL(extra1, extra2) \
	if (!rvalue) rsize = 0; \
  { \
    extra1; \
    ffi_call_e2k(fn, rsize > sizeof(regs_t) ? 0 : rsize, rvalue); \
    extra2; \
  } \
  if (rsize > sizeof(regs_t)) { \
    long i, i2, i1; uint32_t t4, t2, t1; \
    _Pragma("ivdep") \
    for (i = 0; i < rsize - 7; i += 8) \
      *(uint64_t*)((char*)rvalue + i) = *(uint64_t*)((char*)frame + i); \
    i2 = i + (rsize & 4); \
    i1 = i + (rsize & 6); \
    if (rsize & 4) t4 = *(uint32_t*)((char*)frame + i); \
    if (rsize & 2) t2 = *(uint16_t*)((char*)frame + i2); \
    if (rsize & 1) t1 = *(uint8_t*)((char*)frame + i1); \
    if (rsize & 4) *(uint32_t*)((char*)rvalue + i) = t4; \
    if (rsize & 2) *(uint16_t*)((char*)rvalue + i2) = t2;  \
    if (rsize & 1) *(uint8_t*)((char*)rvalue + i1) = t1;  \
  }

#define ASM_FN_START(name) \
  "\t.global " #name "\n" \
  "\t.type " #name ",@function\n" \
  "\t.align 8\n" \
  #name ":\n"

#define ASM_FN_END(name) \
  "\t.size " #name ", .- " #name "\n"

/* Low level routine for calling functions. */
asm(
ASM_FN_START(ffi_call_e2k)
"\t{\n"
"\t  setwd wsz = 8, nfx = 1, dbl = 0\n"
"\t  setbn rbs = 4, rsz = 3, rcur = 0\n"
"\t  movtd,sm %r0, %ctpr1; ipd 2\n"
"\t  shld 1, 5, %r3\n"
"\t}\n"
"\t{\n"
"\t  nop 1\n"
"\t  getsp,0 0, %r0\n"
"\t  andnd %r1, 7, %r4\n"
"\t  andnd %r1, 3, %r5\n"
"\t  andnd %r1, 1, %r6\n"
"\t}\n"
"\t{\n"
"\t  ldd,sm 0, %r0, %b[0]\n"
"\t  ldd,sm 8, %r0, %b[1]\n"
"\t  ldd,sm 16, %r0, %b[2]\n"
"\t  ldd,sm 24, %r0, %b[3]\n"
"\t  addd %r0, %r3, %r7\n"
"\t}\n"
"\t{\n"
"\t  ldd,sm 0, %r7, %b[4]\n"
"\t  ldd,sm 8, %r7, %b[5]\n"
"\t  ldd,sm 16, %r7, %b[6]\n"
"\t  ldd,sm 24, %r7, %b[7]\n"
"\t  subd %r1, %r3, %r0\n"
"\t}\n"
"\t{\n"
"\t  cmpldb 7, %r1, %pred0\n"
"\t  cmpldb 15, %r1, %pred1\n"
"\t  cmpldb 23, %r1, %pred2\n"
"\t  cmpldb 31, %r1, %pred3\n"
"\t  andd %r1, 6, %r7\n"
"\t}\n"
"\t{\n"
"\t  nop 2\n"
"\t  cmpldb 7, %r0, %pred4\n"
"\t  cmpldb 15, %r0, %pred5\n"
"\t  cmpldb 23, %r0, %pred6\n"
"\t  cmpldb 31, %r0, %pred7\n"
"\t  shld %r7, 3, %r7\n"
"\t}\n"
"\t{\n"
"\t  call %ctpr1, wbs = 4; ipd 3\n"
"\t  addd %r2, %r3, %r0\n"
"\t}\n"
"\t{\n"
"\t  return %ctpr3; ipd 2\n"
"\t  addd,1,sm %b[6], 0, %b[7] ? ~%pred6\n"
"\t  std,sm 24, %r0, %b[7] ? %pred7\n"
"\t  std,sm 16, %r0, %b[6] ? %pred6\n"
"\t}\n"
"\t{\n"
"\t  cmpandesb %r1, 4, %pred9\n"
"\t  addd,0,sm %b[5], 0, %b[7] ? ~%pred5\n"
"\t  addd,1,sm %b[4], 0, %b[7] ? ~%pred4\n"
"\t  std,sm 8, %r0, %b[5] ? %pred5\n"
"\t  std,sm 0, %r0, %b[4] ? %pred4\n"
"\t}\n"
"\t{\n"
"\t  cmpandesb %r1, 2, %pred10\n"
"\t  addd,0,sm %b[3], 0, %b[7] ? ~%pred3\n"
"\t  addd,1,sm %b[2], 0, %b[7] ? ~%pred2\n"
"\t  std,sm 24, %r2, %b[3] ? %pred3\n"
"\t  std,sm 16, %r2, %b[2] ? %pred2\n"
"\t}\n"
"\t{\n"
"\t  cmpandesb %r1, 1, %pred11\n"
"\t  addd,0,sm %b[1], 0, %b[7] ? ~%pred1\n"
"\t  addd,1,sm %b[0], 0, %b[7] ? ~%pred0\n"
"\t  std,sm 8, %r2, %b[1] ? %pred1\n"
"\t  std,sm 0, %r2, %b[0] ? %pred0\n"
"\t}\n"
"\t{\n"
"\t  shrd,sm %b[7], %r7, %r7\n"
"\t  stw,sm %r2, %r4, %b[7] ? ~%pred9\n"
"\t  shrd,sm %b[7], %r3, %b[7] ? ~%pred9\n"
"\t}\n"
"\t{\n"
"\t  sth,sm %r2, %r5, %b[7] ? ~%pred10\n"
"\t  stb,sm %r2, %r6, %r7 ? ~%pred11\n"
"\t}\n"
"\t{\n"
"\t  ct %ctpr3; ipd 3\n"
"\t}\n"
ASM_FN_END(ffi_call_e2k)
);

void
ffi_call_e2k(void (*fn)(void), long rsize, void *rvalue);

void
ffi_call(ffi_cif *cif, void (*fn)(void), void *rvalue, void **avalue)
{
  long *frame, *dst, i, size, nargs = cif->nargs, asize = cif->flags;
  long rsize = asize & 1 ? sizeof(ffi_arg) : cif->rtype->size;
  ffi_type **arg_types = cif->arg_types;

  dst = frame = (long*)alloca(asize & -16) - ALLOCA_EXTRA;

  for (i = 0; i < nargs; i++)
    {
      ffi_type *arg_type = arg_types[i];
      void *src = avalue[i];
      size = arg_type->size;
      dst = (long*)FFI_ALIGN(dst, 8);

      switch (arg_type->type)
        {
        case FFI_TYPE_SINT8:
          *dst = *(SINT8*)src;
          break;
        case FFI_TYPE_UINT8:
          *dst = *(UINT8*)src;
          break;
        case FFI_TYPE_SINT16:
          *dst = *(SINT16*)src;
          break;
        case FFI_TYPE_UINT16:
          *dst = *(UINT16*)src;
          break;
        case FFI_TYPE_SINT32:
          *dst = *(SINT32*)src;
          break;
        case FFI_TYPE_UINT32:
        case FFI_TYPE_FLOAT:
          *dst = *(UINT32*)src;
          break;
        case FFI_TYPE_SINT64:
        case FFI_TYPE_UINT64:
        case FFI_TYPE_DOUBLE:
          *dst = *(UINT64*)src;
          break;
        case FFI_TYPE_POINTER:
          *dst = *(uintptr_t*)src;
          break;
        default:
          if (size > 8)
            dst = (long*)FFI_ALIGN(dst, 16);
          if (size & 7)
            *(long*)(((uintptr_t)dst + size) & -8) = 0;
          memcpy(dst, src, size);
        }
      dst = (long*)((uintptr_t)dst + size);
    }

  MAKE_CALL((void)0, (void)0)
}

static regs_t
ffi_closure_e2k()
{
  register void *env __asm__("g16");
  register void *sarg __asm__("g17");
  ffi_closure *closure = env;
  ffi_cif *cif = closure->cif;
  regs_t *ret; void **args; char *ptr;
  long i, size, align, *dst = sarg, nargs;
  ffi_type **arg_types;

  args = alloca(cif->nargs * sizeof(void*));
  size = cif->rtype->size;
  if (size < sizeof(*ret)) size = sizeof(*ret);
  ret = alloca(size);

  if (size & 7)
    *(long*)(((uintptr_t)ret + size) & -8) = 0;

  nargs = cif->nargs;
  arg_types = cif->arg_types;
  for (i = 0; i < nargs; i++)
    {
      ffi_type *arg_type = arg_types[i];
      size = arg_type->size;
      align = size > 8 ? 16 : 8;
      ptr = (char*)FFI_ALIGN(dst, align);
      dst = (long*)(ptr + size);

      if ((uintptr_t)dst > (uintptr_t)sarg + 64)
        ptr += 64;

      args[i] = ptr;
    }
  /* g17 may be changed during the call */
  dst = sarg;

  closure->fun(cif, ret, args, closure->user_data);

  switch (cif->rtype->type)
    {
    case FFI_TYPE_SINT8:
      ret->r[0] = *(SINT8*)ret;
      break;
    case FFI_TYPE_SINT16:
      ret->r[0] = *(SINT16*)ret;
      break;
    case FFI_TYPE_SINT32:
      ret->r[0] = *(SINT32*)ret;
      break;
    default:
      size = cif->rtype->size;
      if (size > sizeof(*ret))
        {
          ptr = (char*)(dst + 8);
          for (i = 0; i < size; i += sizeof(long))
            *(long*)(ptr + i) = *(long*)((char*)ret + i);
        }
    }

  return *ret;
}

ffi_status ffi_prep_closure_loc(ffi_closure* closure, ffi_cif* cif,
    void (*fun)(ffi_cif*, void*, void**, void*),
    void *user_data, void *codeloc)
{
  char *function = closure->tramp;
  void *address = ffi_closure_e2k, *data = closure;

  if (cif->abi <= FFI_FIRST_ABI || cif->abi >= FFI_LAST_ABI)
    return FFI_BAD_ABI;

#define swaphilo(x) ((x) << 32 | (x) >> 32)
  /* function:
   *   movtd,sm <address>, %ctpr1; ipd 2
   *   addd 0, <data>, %g16
   *
   *   nop 1
   *   getsp -64, %g17
   *
   *   std,sm %g17, 0, %r0
   *   std,sm %g17, 8, %r1
   *
   *   std,sm %g17, 16, %r2
   *   std,sm %g17, 24, %r3
   *
   *   std,sm %g17, 32, %r4
   *   std,sm %g17, 40, %r5
   *
   *   nop 2
   *   std,sm %g17, 48, %r6
   *   std,sm %g17, 56, %r7
   *
   *   ct %ctpr1
   */
  {
    long *p = (long*)function;
    p[0] = 0x800000000C001033;
    p[1] = 0x11C0DCF0E1C0DED1;
    p[2] = swaphilo((unsigned long) address);
    p[3] = swaphilo((unsigned long) data);
    p[4] = 0x58ECD4F104100091;
    p[5] = 0xFFC0000001C00000;
    p += 6;
    p[0] = 0xA7F1D48090000012;
    p[1] = 0x00000008A7F1D081;
    p[2] = 0xA7F1D48290000012;
    p[3] = 0x00100018A7F1D083;
    p[4] = 0xA7F1D48490000012;
    p[5] = 0x00200028A7F1D085;
    p[6] = 0xA7F1D48690000112;
    p[7] = 0x00300038A7F1D087;
    p[8] = 0xC000042000001001;
    /* size = (6+9)*8 = 120 */
  }

  closure->cif = cif;
  closure->fun = fun;
  closure->user_data = user_data;

  __builtin___clear_cache(function, function + FFI_TRAMPOLINE_SIZE);

  return FFI_OK;
}

/* set g16 = closure before calling fn() */
void ffi_call_go(ffi_cif *cif, void (*fn)(void), void *rvalue,
    void **avalue, void *closure)
{
  /* copy from ffi_call */
  long *frame, *dst, i, size, nargs = cif->nargs, asize = cif->flags;
  long rsize = asize & 1 ? sizeof(ffi_arg) : cif->rtype->size;
  ffi_type **arg_types = cif->arg_types;

  dst = frame = (long*)alloca(asize & -16) - ALLOCA_EXTRA;

  for (i = 0; i < nargs; i++)
    {
      ffi_type *arg_type = arg_types[i];
      void *src = avalue[i];
      size = arg_type->size;
      dst = (long*)FFI_ALIGN(dst, 8);

      switch (arg_type->type)
        {
        case FFI_TYPE_SINT8:
          *dst = *(SINT8*)src;
          break;
        case FFI_TYPE_UINT8:
          *dst = *(UINT8*)src;
          break;
        case FFI_TYPE_SINT16:
          *dst = *(SINT16*)src;
          break;
        case FFI_TYPE_UINT16:
          *dst = *(UINT16*)src;
          break;
        case FFI_TYPE_SINT32:
          *dst = *(SINT32*)src;
          break;
        case FFI_TYPE_UINT32:
        case FFI_TYPE_FLOAT:
          *dst = *(UINT32*)src;
          break;
        case FFI_TYPE_SINT64:
        case FFI_TYPE_UINT64:
        case FFI_TYPE_DOUBLE:
          *dst = *(UINT64*)src;
          break;
        case FFI_TYPE_POINTER:
          *dst = *(uintptr_t*)src;
          break;
        default:
          if (size > 8) dst = (long*)FFI_ALIGN(dst, 16);
          if (size & 7)
            *(long*)(((uintptr_t)dst + size) & -8) = 0;
          memcpy(dst, src, size);
        }
      dst = (long*)((uintptr_t)dst + size);
    }
  /* copy end */
  MAKE_CALL(
    register void *chain_reg __asm__("g16") = closure,
    __asm__ __volatile__ ("" : : "r"(chain_reg) : "memory")
  )
}

/* with ffi_go_closure, user_data = closure */
static regs_t ffi_go_closure_e2k()
{
  register void *env __asm__("g16");
  register long *sarg __asm__("g17");
  ffi_go_closure *closure = env;
  /* copy from ffi_closure */
  ffi_cif *cif = closure->cif;
  regs_t *ret; void **args; char *ptr;
  long i, size, align, *dst = sarg, nargs;
  ffi_type **arg_types;

  args = alloca(cif->nargs * sizeof(void*));
  size = cif->rtype->size;
  if (size < sizeof(*ret)) size = sizeof(*ret);
  ret = alloca(size);

  if (size & 7)
    *(long*)(((uintptr_t)ret + size) & -8) = 0;

  nargs = cif->nargs;
  arg_types = cif->arg_types;
  for (i = 0; i < nargs; i++)
    {
      ffi_type *arg_type = arg_types[i];
      size = arg_type->size;
      align = size > 8 ? 16 : 8;
      ptr = (char*)FFI_ALIGN(dst, align);
      dst = (long*)(ptr + size);

      if ((uintptr_t)dst > (uintptr_t)sarg + 64)
        ptr += 64;

      args[i] = ptr;
    }
  /* g17 may be changed during the call */
  dst = sarg;
  /* copy end */

  closure->fun(cif, ret, args, closure);

  /* copy from ffi_closure */
  switch (cif->rtype->type)
    {
    case FFI_TYPE_SINT8:
      ret->r[0] = *(SINT8*)ret;
      break;
    case FFI_TYPE_SINT16:
      ret->r[0] = *(SINT16*)ret;
      break;
    case FFI_TYPE_SINT32:
      ret->r[0] = *(SINT32*)ret;
      break;
    default:
      size = cif->rtype->size;
      if (size > sizeof(*ret))
        {
          ptr = (char*)(dst + 8);
          for (i = 0; i < size; i += sizeof(long))
            *(long*)(ptr + i) = *(long*)((char*)ret + i);
        }
    }

  return *ret;
  /* copy end */
}

ffi_status
ffi_prep_go_closure(ffi_go_closure* closure, ffi_cif* cif,
    void (*fun)(ffi_cif*, void*, void**, void*))
{
  closure->tramp = ffi_go_closure_e2k;
  closure->cif = cif;
  closure->fun = fun;

  if (cif->abi <= FFI_FIRST_ABI || cif->abi >= FFI_LAST_ABI)
    return FFI_BAD_ABI;

  return FFI_OK;
}

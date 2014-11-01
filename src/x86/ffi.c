/* -----------------------------------------------------------------------
   ffi.c - Copyright (c) 1996, 1998, 1999, 2001, 2007, 2008  Red Hat, Inc.
           Copyright (c) 2002  Ranjit Mathew
           Copyright (c) 2002  Bo Thorsen
           Copyright (c) 2002  Roger Sayle
           Copyright (C) 2008, 2010  Free Software Foundation, Inc.

   x86 Foreign Function Interface

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

#ifndef __x86_64__
#include <ffi.h>
#include <ffi_common.h>
#include <stdlib.h>
#include "internal.h"

/* Force FFI_TYPE_LONGDOUBLE to be different than FFI_TYPE_DOUBLE;
   all further uses in this file will refer to the 80-bit type.  */
#if FFI_TYPE_LONGDOUBLE != FFI_TYPE_DOUBLE
# if FFI_TYPE_LONGDOUBLE != 4
#  error FFI_TYPE_LONGDOUBLE out of date
# endif
#else
# undef FFI_TYPE_LONGDOUBLE
# define FFI_TYPE_LONGDOUBLE 4
#endif

#if defined(__GNUC__) && !defined(__declspec)
# define __declspec(x)  __attribute__((x))
#endif

/* Perform machine dependent cif processing.  */
ffi_status FFI_HIDDEN
ffi_prep_cif_machdep(ffi_cif *cif)
{
  size_t bytes = 0;
  int i, n, flags, cabi = cif->abi;

  switch (cabi)
    {
    case FFI_SYSV:
    case FFI_STDCALL:
    case FFI_THISCALL:
    case FFI_FASTCALL:
    case FFI_MS_CDECL:
    case FFI_PASCAL:
    case FFI_REGISTER:
      break;
    default:
      return FFI_BAD_ABI;
    }

  switch (cif->rtype->type)
    {
    case FFI_TYPE_VOID:
      flags = X86_RET_VOID;
      break;
    case FFI_TYPE_FLOAT:
      flags = X86_RET_FLOAT;
      break;
    case FFI_TYPE_DOUBLE:
      flags = X86_RET_DOUBLE;
      break;
    case FFI_TYPE_LONGDOUBLE:
      flags = X86_RET_LDOUBLE;
      break;
    case FFI_TYPE_UINT8:
      flags = X86_RET_UINT8;
      break;
    case FFI_TYPE_UINT16:
      flags = X86_RET_UINT16;
      break;
    case FFI_TYPE_SINT8:
      flags = X86_RET_SINT8;
      break;
    case FFI_TYPE_SINT16:
      flags = X86_RET_SINT16;
      break;
    case FFI_TYPE_INT:
    case FFI_TYPE_SINT32:
    case FFI_TYPE_UINT32:
    case FFI_TYPE_POINTER:
      flags = X86_RET_INT32;
      break;
    case FFI_TYPE_SINT64:
    case FFI_TYPE_UINT64:
      flags = X86_RET_INT64;
      break;
    case FFI_TYPE_STRUCT:
#ifndef X86
      /* ??? This should be a different ABI rather than an ifdef.  */
      if (cif->rtype->size == 1)
	flags = X86_RET_STRUCT_1B;
      else if (cif->rtype->size == 2)
	flags = X86_RET_STRUCT_2B;
      else if (cif->rtype->size == 4)
	flags = X86_RET_INT32;
      else if (cif->rtype->size == 8)
	flags = X86_RET_INT64;
      else
#endif
	{
	  switch (cabi)
	    {
	    case FFI_THISCALL:
	    case FFI_FASTCALL:
	    case FFI_STDCALL:
	    case FFI_MS_CDECL:
	      flags = X86_RET_STRUCTARG;
	      break;
	    default:
	      flags = X86_RET_STRUCTPOP;
	      break;
	    }
	  /* Allocate space for return value pointer.  */
	  bytes += ALIGN (sizeof(void*), FFI_SIZEOF_ARG);
	}
      break;
    default:
      return FFI_BAD_TYPEDEF;
    }
  cif->flags = flags;

  for (i = 0, n = cif->nargs; i < n; i++)
    {
      ffi_type *t = cif->arg_types[i];

      bytes = ALIGN (bytes, t->alignment);
      bytes += ALIGN (t->size, FFI_SIZEOF_ARG);
    }
  cif->bytes = ALIGN (bytes, 16);

  return FFI_OK;
}

static ffi_arg
extend_basic_type(void *arg, int type)
{
  switch (type)
    {
    case FFI_TYPE_SINT8:
      return *(SINT8 *)arg;
    case FFI_TYPE_UINT8:
      return *(UINT8 *)arg;
    case FFI_TYPE_SINT16:
      return *(SINT16 *)arg;
    case FFI_TYPE_UINT16:
      return *(UINT16 *)arg;

    case FFI_TYPE_SINT32:
    case FFI_TYPE_UINT32:
    case FFI_TYPE_POINTER:
    case FFI_TYPE_FLOAT:
      return *(UINT32 *)arg;

    default:
      abort();
    }
}

struct call_frame
{
  void *ebp;		/* 0 */
  void *retaddr;	/* 4 */
  void (*fn)(void);	/* 8 */
  int flags;		/* 12 */
  void *rvalue;		/* 16 */
  unsigned regs[3];	/* 20-28 */
};

struct abi_params
{
  int dir;		/* parameter growth direction */
  int nregs;		/* number of register parameters */
  int regs[3];
};

static const struct abi_params abi_params[FFI_LAST_ABI] = {
  [FFI_SYSV] = { 1, 0 },
  [FFI_THISCALL] = { 1, 1, { R_ECX } },
  [FFI_FASTCALL] = { 1, 2, { R_ECX, R_EDX } },
  [FFI_STDCALL] = { 1, 0 },
  [FFI_PASCAL] = { -1, 0 },
  [FFI_REGISTER] = { -1, 3, { R_EAX, R_EDX, R_ECX } },
  [FFI_MS_CDECL] = { 1, 0 }
};

extern void ffi_call_i386(struct call_frame *, char *)
	FFI_HIDDEN __declspec(fastcall);

void
ffi_call (ffi_cif *cif, void (*fn)(void), void *rvalue, void **avalue)
{
  size_t rsize, bytes;
  struct call_frame *frame;
  char *stack, *argp;
  ffi_type **arg_types;
  int flags, cabi, i, n, dir, narg_reg;
  const struct abi_params *pabi;

  flags = cif->flags;
  cabi = cif->abi;
  pabi = &abi_params[cabi];
  dir = pabi->dir;

  rsize = 0;
  if (rvalue == NULL)
    {
      switch (flags)
	{
	case X86_RET_FLOAT:
	case X86_RET_DOUBLE:
	case X86_RET_LDOUBLE:
	case X86_RET_STRUCTPOP:
	case X86_RET_STRUCTARG:
	  /* The float cases need to pop the 387 stack.
	     The struct cases need to pass a valid pointer to the callee.  */
	  rsize = cif->rtype->size;
	  break;
	default:
	  /* We can pretend that the callee returns nothing.  */
	  flags = X86_RET_VOID;
	  break;
	}
    }

  bytes = cif->bytes;
  stack = alloca(bytes + sizeof(*frame) + rsize);
  argp = (dir < 0 ? stack + bytes : stack);
  frame = (struct call_frame *)(stack + bytes);
  if (rsize)
    rvalue = frame + 1;

  frame->fn = fn;
  frame->flags = flags;
  frame->rvalue = rvalue;

  narg_reg = 0;
  switch (flags)
    {
    case X86_RET_STRUCTARG:
      /* The pointer is passed as the first argument.  */
      if (pabi->nregs > 0)
	{
	  frame->regs[pabi->regs[0]] = (unsigned)rvalue;
	  narg_reg = 1;
	  break;
	}
      /* fallthru */
    case X86_RET_STRUCTPOP:
      *(void **)argp = rvalue;
      argp += sizeof(void *);
      break;
    }

  arg_types = cif->arg_types;
  for (i = 0, n = cif->nargs; i < n; i++)
    {
      ffi_type *ty = arg_types[i];
      void *valp = avalue[i];
      size_t z = ty->size;
      int t = ty->type;

      if (z <= FFI_SIZEOF_ARG && t != FFI_TYPE_STRUCT)
        {
	  ffi_arg val = extend_basic_type (valp, t);

	  if (t != FFI_TYPE_FLOAT && narg_reg < pabi->nregs)
	    frame->regs[pabi->regs[narg_reg++]] = val;
	  else if (dir < 0)
	    {
	      argp -= 4;
	      *(ffi_arg *)argp = val;
	    }
	  else
	    {
	      *(ffi_arg *)argp = val;
	      argp += 4;
	    }
	}
      else
	{
	  size_t za = ALIGN (z, FFI_SIZEOF_ARG);
	  if (dir < 0)
	    {
	      argp -= za;
	      memcpy (argp, valp, z);
	    }
	  else
	    {
	      memcpy (argp, valp, z);
	      argp += za;
	    }
	}
    }
  FFI_ASSERT (dir > 0 || argp == stack);

  ffi_call_i386 (frame, stack);
}


/** private members **/

/* The following __attribute__((regparm(1))) decorations will have no effect
   on MSVC or SUNPRO_C -- standard conventions apply. */
static unsigned int ffi_prep_incoming_args (char *stack, void **ret,
                                            void** args, ffi_cif* cif);
void FFI_HIDDEN ffi_closure_SYSV (ffi_closure *)
     __attribute__ ((regparm(1)));
unsigned int FFI_HIDDEN ffi_closure_SYSV_inner (ffi_closure *, void **, void *)
     __attribute__ ((regparm(1)));
unsigned int FFI_HIDDEN ffi_closure_WIN32_inner (ffi_closure *, void **, void *)
     __attribute__ ((regparm(1)));
void FFI_HIDDEN ffi_closure_raw_SYSV (ffi_raw_closure *)
     __attribute__ ((regparm(1)));
#ifdef X86_WIN32
void FFI_HIDDEN ffi_closure_raw_THISCALL (ffi_raw_closure *)
     __attribute__ ((regparm(1)));
#endif
void FFI_HIDDEN ffi_closure_STDCALL (ffi_closure *);
void FFI_HIDDEN ffi_closure_THISCALL (ffi_closure *);
void FFI_HIDDEN ffi_closure_FASTCALL (ffi_closure *);
void FFI_HIDDEN ffi_closure_REGISTER (ffi_closure *);

/* This function is jumped to by the trampoline */

unsigned int FFI_HIDDEN __attribute__ ((regparm(1)))
ffi_closure_SYSV_inner (ffi_closure *closure, void **respp, void *args)
{
  /* our various things...  */
  ffi_cif       *cif;
  void         **arg_area;

  cif         = closure->cif;
  arg_area    = (void**) alloca (cif->nargs * sizeof (void*));  

  /* this call will initialize ARG_AREA, such that each
   * element in that array points to the corresponding 
   * value on the stack; and if the function returns
   * a structure, it will change RESP to point to the
   * structure return address.  */

  ffi_prep_incoming_args(args, respp, arg_area, cif);

  (closure->fun) (cif, *respp, arg_area, closure->user_data);

  return cif->flags;
}

unsigned int FFI_HIDDEN __attribute__ ((regparm(1)))
ffi_closure_WIN32_inner (ffi_closure *closure, void **respp, void *args)
{
  /* our various things...  */
  ffi_cif       *cif;
  void         **arg_area;
  unsigned int   ret;

  cif         = closure->cif;
  arg_area    = (void**) alloca (cif->nargs * sizeof (void*));  

  /* this call will initialize ARG_AREA, such that each
   * element in that array points to the corresponding 
   * value on the stack; and if the function returns
   * a structure, it will change RESP to point to the
   * structure return address.  */

  ret = ffi_prep_incoming_args(args, respp, arg_area, cif);

  (closure->fun) (cif, *respp, arg_area, closure->user_data);

  return ret;
}

static unsigned int
ffi_prep_incoming_args(char *stack, void **rvalue, void **avalue,
                       ffi_cif *cif)
{
  register unsigned int i;
  register void **p_argv;
  register char *argp;
  register ffi_type **p_arg;
  const int cabi = cif->abi;
  const int dir = (cabi == FFI_PASCAL || cabi == FFI_REGISTER) ? -1 : +1;
  const unsigned int max_stack_count = (cabi == FFI_THISCALL) ? 1
                                     : (cabi == FFI_FASTCALL) ? 2
                                     : (cabi == FFI_REGISTER) ? 3
                                     : 0;
  unsigned int passed_regs = 0;
  void *p_stack_data[3] = { stack - 1 };

  argp = stack;
  argp += max_stack_count * FFI_SIZEOF_ARG;

  if ((cif->flags == FFI_TYPE_STRUCT
       || cif->flags == FFI_TYPE_MS_STRUCT))
    {
      if (passed_regs < max_stack_count)
        {
          *rvalue = *(void**) (stack + (passed_regs*FFI_SIZEOF_ARG));
          ++passed_regs;
        }
      else
        {
          *rvalue = *(void **) argp;
          argp += sizeof(void *);
        }
    }

  /* Do register arguments first  */
  for (i = 0, p_arg = cif->arg_types; 
       i < cif->nargs && passed_regs < max_stack_count;
       i++, p_arg++)
    {
      if ((*p_arg)->type == FFI_TYPE_FLOAT
         || (*p_arg)->type == FFI_TYPE_STRUCT)
        continue;

      size_t sz = (*p_arg)->size;
      if(sz == 0 || sz > FFI_SIZEOF_ARG)
        continue;

      p_stack_data[passed_regs] = avalue + i;
      avalue[i] = stack + (passed_regs*FFI_SIZEOF_ARG);
      ++passed_regs;
    }

  p_arg = cif->arg_types;
  p_argv = avalue;
  if (dir < 0)
    {
      const int nargs = cif->nargs - 1;
      if (nargs > 0)
      {
        p_arg  += nargs;
        p_argv += nargs;
      }
    }

  for (i = cif->nargs;
       i != 0;
       i--, p_arg += dir, p_argv += dir)
    {
      /* Align if necessary */
      if ((sizeof(void*) - 1) & (size_t) argp)
        argp = (char *) ALIGN(argp, sizeof(void*));

      size_t z = (*p_arg)->size;

      if (passed_regs > 0
          && z <= FFI_SIZEOF_ARG
          && (p_argv == p_stack_data[0]
            || p_argv == p_stack_data[1]
            || p_argv == p_stack_data[2]))
        {
          /* Already assigned a register value */
          continue;
        }
      else
        {
          /* because we're little endian, this is what it turns into.   */
          *p_argv = (void*) argp;
        }

      argp += z;
    }

  return (size_t)argp - (size_t)stack;
}

/* How to make a trampoline.  Derived from gcc/config/i386/i386.c. */

#define FFI_INIT_TRAMPOLINE(TRAMP,FUN,CTX) \
{ unsigned char *__tramp = (unsigned char*)(TRAMP); \
   unsigned int  __fun = (unsigned int)(FUN); \
   unsigned int  __ctx = (unsigned int)(CTX); \
   unsigned int  __dis = __fun - (__ctx + 10);  \
   *(unsigned char*) &__tramp[0] = 0xb8; \
   *(unsigned int*)  &__tramp[1] = __ctx; /* movl __ctx, %eax */ \
   *(unsigned char*) &__tramp[5] = 0xe9; \
   *(unsigned int*)  &__tramp[6] = __dis; /* jmp __fun  */ \
 }

#define FFI_INIT_TRAMPOLINE_RAW_THISCALL(TRAMP,FUN,CTX,SIZE) \
{ unsigned char *__tramp = (unsigned char*)(TRAMP); \
   unsigned int  __fun = (unsigned int)(FUN); \
   unsigned int  __ctx = (unsigned int)(CTX); \
   unsigned int  __dis = __fun - (__ctx + 49);  \
   unsigned short __size = (unsigned short)(SIZE); \
   *(unsigned int *) &__tramp[0] = 0x8324048b;      /* mov (%esp), %eax */ \
   *(unsigned int *) &__tramp[4] = 0x4c890cec;      /* sub $12, %esp */ \
   *(unsigned int *) &__tramp[8] = 0x04890424;      /* mov %ecx, 4(%esp) */ \
   *(unsigned char*) &__tramp[12] = 0x24;           /* mov %eax, (%esp) */ \
   *(unsigned char*) &__tramp[13] = 0xb8; \
   *(unsigned int *) &__tramp[14] = __size;         /* mov __size, %eax */ \
   *(unsigned int *) &__tramp[18] = 0x08244c8d;     /* lea 8(%esp), %ecx */ \
   *(unsigned int *) &__tramp[22] = 0x4802e8c1;     /* shr $2, %eax ; dec %eax */ \
   *(unsigned short*) &__tramp[26] = 0x0b74;        /* jz 1f */ \
   *(unsigned int *) &__tramp[28] = 0x8908518b;     /* 2b: mov 8(%ecx), %edx */ \
   *(unsigned int *) &__tramp[32] = 0x04c18311;     /* mov %edx, (%ecx) ; add $4, %ecx */ \
   *(unsigned char*) &__tramp[36] = 0x48;           /* dec %eax */ \
   *(unsigned short*) &__tramp[37] = 0xf575;        /* jnz 2b ; 1f: */ \
   *(unsigned char*) &__tramp[39] = 0xb8; \
   *(unsigned int*)  &__tramp[40] = __ctx;          /* movl __ctx, %eax */ \
   *(unsigned char *)  &__tramp[44] = 0xe8; \
   *(unsigned int*)  &__tramp[45] = __dis;          /* call __fun  */ \
   *(unsigned char*)  &__tramp[49] = 0xc2;          /* ret  */ \
   *(unsigned short*)  &__tramp[50] = (__size + 8); /* ret (__size + 8)  */ \
 }

#define FFI_INIT_TRAMPOLINE_WIN32(TRAMP,FUN,CTX)  \
{ unsigned char *__tramp = (unsigned char*)(TRAMP); \
   unsigned int  __fun = (unsigned int)(FUN); \
   unsigned int  __ctx = (unsigned int)(CTX); \
   unsigned int  __dis = __fun - (__ctx + 10); \
   *(unsigned char*) &__tramp[0] = 0x68; \
   *(unsigned int*)  &__tramp[1] = __ctx; /* push __ctx */ \
   *(unsigned char*) &__tramp[5] = 0xe9; \
   *(unsigned int*)  &__tramp[6] = __dis; /* jmp __fun  */ \
 }

/* the cif must already be prep'ed */

ffi_status
ffi_prep_closure_loc (ffi_closure* closure,
                      ffi_cif* cif,
                      void (*fun)(ffi_cif*,void*,void**,void*),
                      void *user_data,
                      void *codeloc)
{
  if (cif->abi == FFI_SYSV)
    {
      FFI_INIT_TRAMPOLINE (&closure->tramp[0],
                           &ffi_closure_SYSV,
                           (void*)codeloc);
    }
  else if (cif->abi == FFI_REGISTER)
    {
      FFI_INIT_TRAMPOLINE_WIN32 (&closure->tramp[0],
                                   &ffi_closure_REGISTER,
                                   (void*)codeloc);
    }
  else if (cif->abi == FFI_FASTCALL)
    {
      FFI_INIT_TRAMPOLINE_WIN32 (&closure->tramp[0],
                                   &ffi_closure_FASTCALL,
                                   (void*)codeloc);
    }
  else if (cif->abi == FFI_THISCALL)
    {
      FFI_INIT_TRAMPOLINE_WIN32 (&closure->tramp[0],
                                   &ffi_closure_THISCALL,
                                   (void*)codeloc);
    }
  else if (cif->abi == FFI_STDCALL || cif->abi == FFI_PASCAL)
    {
      FFI_INIT_TRAMPOLINE_WIN32 (&closure->tramp[0],
                                   &ffi_closure_STDCALL,
                                   (void*)codeloc);
    }
  else if (cif->abi == FFI_MS_CDECL)
    {
      FFI_INIT_TRAMPOLINE (&closure->tramp[0],
                           &ffi_closure_SYSV,
                           (void*)codeloc);
    }
  else
    {
      return FFI_BAD_ABI;
    }
    
  closure->cif  = cif;
  closure->user_data = user_data;
  closure->fun  = fun;

  return FFI_OK;
}

/* ------- Native raw API support -------------------------------- */

#if !FFI_NO_RAW_API

ffi_status
ffi_prep_raw_closure_loc (ffi_raw_closure* closure,
                          ffi_cif* cif,
                          void (*fun)(ffi_cif*,void*,ffi_raw*,void*),
                          void *user_data,
                          void *codeloc)
{
  int i;

  /* We currently don't support certain kinds of arguments for raw
     closures.  This should be implemented by a separate assembly
     language routine, since it would require argument processing,
     something we don't do now for performance.  */
  for (i = cif->nargs-1; i >= 0; i--)
    {
      FFI_ASSERT (cif->arg_types[i]->type != FFI_TYPE_STRUCT);
      FFI_ASSERT (cif->arg_types[i]->type != FFI_TYPE_LONGDOUBLE);
    }

  switch (cif->abi)
    {
#ifdef X86_WIN32
    case FFI_THISCALL:
      FFI_INIT_TRAMPOLINE_RAW_THISCALL (&closure->tramp[0],
					&ffi_closure_raw_THISCALL,
					codeloc, cif->bytes);
      break;
#endif
    case FFI_SYSV:
      FFI_INIT_TRAMPOLINE (&closure->tramp[0], &ffi_closure_raw_SYSV,
			   codeloc);
      break;
    default:
      return FFI_BAD_ABI;
    }

  closure->cif = cif;
  closure->fun = fun;
  closure->user_data = user_data;

  return FFI_OK;
}

void
ffi_raw_call(ffi_cif *cif, void (*fn)(void), void *rvalue, ffi_raw *avalue)
{
  size_t rsize, bytes;
  struct call_frame *frame;
  char *stack, *argp;
  ffi_type **arg_types;
  int flags, cabi, i, n, narg_reg;
  const struct abi_params *pabi;

  flags = cif->flags;
  cabi = cif->abi;
  pabi = &abi_params[cabi];

  rsize = 0;
  if (rvalue == NULL)
    {
      switch (flags)
	{
	case X86_RET_FLOAT:
	case X86_RET_DOUBLE:
	case X86_RET_LDOUBLE:
	case X86_RET_STRUCTPOP:
	case X86_RET_STRUCTARG:
	  /* The float cases need to pop the 387 stack.
	     The struct cases need to pass a valid pointer to the callee.  */
	  rsize = cif->rtype->size;
	  break;
	default:
	  /* We can pretend that the callee returns nothing.  */
	  flags = X86_RET_VOID;
	  break;
	}
    }

  bytes = cif->bytes;
  argp = stack = alloca(bytes + sizeof(*frame) + rsize);
  frame = (struct call_frame *)(stack + bytes);
  if (rsize)
    rvalue = frame + 1;

  narg_reg = 0;
  switch (flags)
    {
    case X86_RET_STRUCTARG:
      /* The pointer is passed as the first argument.  */
      if (pabi->nregs > 0)
	{
	  frame->regs[pabi->regs[0]] = (unsigned)rvalue;
	  narg_reg = 1;
	  break;
	}
      /* fallthru */
    case X86_RET_STRUCTPOP:
      *(void **)argp = rvalue;
      argp += sizeof(void *);
      bytes -= sizeof(void *);
      break;
    }

  arg_types = cif->arg_types;
  for (i = 0, n = cif->nargs; narg_reg < pabi->nregs && i < n; i++)
    {
      ffi_type *ty = arg_types[i];
      size_t z = ty->size;
      int t = ty->type;

      if (z <= FFI_SIZEOF_ARG && t != FFI_TYPE_STRUCT && t != FFI_TYPE_FLOAT)
	{
	  ffi_arg val = extend_basic_type (avalue, t);
	  frame->regs[pabi->regs[narg_reg++]] = val;
	  z = FFI_SIZEOF_ARG;
	}
      else
	{
	  memcpy (argp, avalue, z);
	  z = ALIGN (z, FFI_SIZEOF_ARG);
	  argp += z;
	}
      avalue += z;
      bytes -= z;
    }
  if (i < n)
    memcpy (argp, avalue, bytes);

  ffi_call_i386 (frame, stack);
}
#endif /* !FFI_NO_RAW_API */
#endif /* !__x86_64__ */

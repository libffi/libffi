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


/* ffi_prep_args is called by the assembly routine once stack space
   has been allocated for the function's arguments */

unsigned int ffi_prep_args(char *stack, extended_cif *ecif);
unsigned int ffi_prep_args(char *stack, extended_cif *ecif)
{
  register unsigned int i;
  register void **p_argv;
  register char *argp;
  register ffi_type **p_arg;
  const int cabi = ecif->cif->abi;
  const int dir = (cabi == FFI_PASCAL || cabi == FFI_REGISTER) ? -1 : +1;
  unsigned int stack_args_count = 0;
  void *p_stack_data[3];
  char *argp2 = stack;

  argp = stack;

  if ((ecif->cif->flags == FFI_TYPE_STRUCT
       || ecif->cif->flags == FFI_TYPE_MS_STRUCT))
    {
      /* For fastcall/thiscall/register this is first register-passed
         argument.  */
      if (cabi == FFI_THISCALL || cabi == FFI_FASTCALL || cabi == FFI_REGISTER)
        {
          p_stack_data[stack_args_count] = argp;
          ++stack_args_count;
        }

      *(void **) argp = ecif->rvalue;
      argp += sizeof(void*);
    }

  p_arg  = ecif->cif->arg_types;
  p_argv = ecif->avalue;
  if (dir < 0)
    {
      const int nargs = ecif->cif->nargs - 1;
      if (nargs > 0)
      {
        p_arg  += nargs;
        p_argv += nargs;
      }
    }

  for (i = ecif->cif->nargs;
       i != 0;
       i--, p_arg += dir, p_argv += dir)
    {
      /* Align if necessary */
      if ((sizeof(void*) - 1) & (size_t) argp)
        argp = (char *) ALIGN(argp, sizeof(void*));

      size_t z = (*p_arg)->size;

      if (z < FFI_SIZEOF_ARG)
        {
          z = FFI_SIZEOF_ARG;
          switch ((*p_arg)->type)
            {
            case FFI_TYPE_SINT8:
              *(ffi_sarg *) argp = (ffi_sarg)*(SINT8 *)(* p_argv);
              break;

            case FFI_TYPE_UINT8:
              *(ffi_arg *) argp = (ffi_arg)*(UINT8 *)(* p_argv);
              break;

            case FFI_TYPE_SINT16:
              *(ffi_sarg *) argp = (ffi_sarg)*(SINT16 *)(* p_argv);
              break;

            case FFI_TYPE_UINT16:
              *(ffi_arg *) argp = (ffi_arg)*(UINT16 *)(* p_argv);
              break;

            case FFI_TYPE_SINT32:
              *(ffi_sarg *) argp = (ffi_sarg)*(SINT32 *)(* p_argv);
              break;

            case FFI_TYPE_UINT32:
              *(ffi_arg *) argp = (ffi_arg)*(UINT32 *)(* p_argv);
              break;

            case FFI_TYPE_STRUCT:
              *(ffi_arg *) argp = *(ffi_arg *)(* p_argv);
              break;

            default:
              FFI_ASSERT(0);
            }
        }
      else
        {
          memcpy(argp, *p_argv, z);
        }

    /* For thiscall/fastcall/register convention register-passed arguments
       are the first two none-floating-point arguments with a size
       smaller or equal to sizeof (void*).  */
    if ((z == FFI_SIZEOF_ARG)
        && ((cabi == FFI_REGISTER)
          || (cabi == FFI_THISCALL && stack_args_count < 1)
          || (cabi == FFI_FASTCALL && stack_args_count < 2))
        && ((*p_arg)->type != FFI_TYPE_FLOAT && (*p_arg)->type != FFI_TYPE_STRUCT)
       )
      {
        if (dir < 0 && stack_args_count > 2)
          {
            /* Iterating arguments backwards, so first register-passed argument
               will be passed last. Shift temporary values to make place. */
            p_stack_data[0] = p_stack_data[1];
            p_stack_data[1] = p_stack_data[2];
            stack_args_count = 2;
          }

        p_stack_data[stack_args_count] = argp;
        ++stack_args_count;
      }

      argp += z;
    }

  /* We need to move the register-passed arguments for thiscall,
     fastcall, register on top of stack, so that those can be moved
     to registers by call-handler.  */
  if (stack_args_count > 0)
    {
      if (dir < 0 && stack_args_count > 1)
        {
          /* Reverse order if iterating arguments backwards */
          ffi_arg tmp = *(ffi_arg*) p_stack_data[0];
          *(ffi_arg*) p_stack_data[0] = *(ffi_arg*) p_stack_data[stack_args_count - 1];
          *(ffi_arg*) p_stack_data[stack_args_count - 1] = tmp;
        }
      
      int i;
      for (i = 0; i < stack_args_count; i++)
        {
          if (p_stack_data[i] != argp2)
            {
              ffi_arg tmp = *(ffi_arg*) p_stack_data[i];
              memmove (argp2 + FFI_SIZEOF_ARG, argp2, (size_t) ((char*) p_stack_data[i] - (char*)argp2));
              *(ffi_arg *) argp2 = tmp;
            }

          argp2 += FFI_SIZEOF_ARG;
        }
    }

    return stack_args_count;
    return 0;
}

/* Perform machine dependent cif processing */
ffi_status ffi_prep_cif_machdep(ffi_cif *cif)
{
  unsigned int i;
  ffi_type **ptr;

  /* Set the return type flag */
  switch (cif->rtype->type)
    {
    case FFI_TYPE_VOID:
    case FFI_TYPE_UINT8:
    case FFI_TYPE_UINT16:
    case FFI_TYPE_SINT8:
    case FFI_TYPE_SINT16:
    case FFI_TYPE_SINT64:
    case FFI_TYPE_FLOAT:
    case FFI_TYPE_DOUBLE:
    case FFI_TYPE_LONGDOUBLE:
      cif->flags = (unsigned) cif->rtype->type;
      break;

    case FFI_TYPE_UINT64:
      cif->flags = FFI_TYPE_SINT64;
      break;

    case FFI_TYPE_STRUCT:
#ifndef X86
      /* ??? This should be a different ABI rather than an ifdef.  */
      if (cif->rtype->size == 1)
	cif->flags = FFI_TYPE_SMALL_STRUCT_1B;	/* same as char size */
      else if (cif->rtype->size == 2)
	cif->flags = FFI_TYPE_SMALL_STRUCT_2B;	/* same as short size */
      else if (cif->rtype->size == 4)
	cif->flags = FFI_TYPE_INT;		/* same as int type */
      else if (cif->rtype->size == 8)
	cif->flags = FFI_TYPE_SINT64;		/* same as int64 type */
      else
#endif
	{
	  if (cif->abi == FFI_MS_CDECL)
	    cif->flags = FFI_TYPE_MS_STRUCT;
	  else
	    cif->flags = FFI_TYPE_STRUCT;
	  /* Allocate space for return value pointer.  */
	  cif->bytes += ALIGN(sizeof(void*), FFI_SIZEOF_ARG);
	}
      break;

    default:
      cif->flags = FFI_TYPE_INT;
      break;
    }

  for (ptr = cif->arg_types, i = cif->nargs; i > 0; i--, ptr++)
    {
      if (((*ptr)->alignment - 1) & cif->bytes)
        cif->bytes = ALIGN(cif->bytes, (*ptr)->alignment);
      cif->bytes += (unsigned)ALIGN((*ptr)->size, FFI_SIZEOF_ARG);
    }

  if (cif->abi == FFI_SYSV)
    cif->bytes = ALIGN (cif->bytes, 15);

  return FFI_OK;
}

extern void
ffi_call_win32(unsigned int (*)(char *, extended_cif *), extended_cif *,
               unsigned, unsigned, unsigned, unsigned *, void (*fn)(void));
extern void ffi_call_SYSV(void (*)(char *, extended_cif *), extended_cif *,
                          unsigned, unsigned, unsigned *, void (*fn)(void));

void ffi_call(ffi_cif *cif, void (*fn)(void), void *rvalue, void **avalue)
{
  extended_cif ecif;

  ecif.cif = cif;
  ecif.avalue = avalue;
  
  /* If the return value is a struct and we don't have a return */
  /* value address then we need to make one                     */

  if (rvalue == NULL
      && (cif->flags == FFI_TYPE_STRUCT
          || cif->flags == FFI_TYPE_MS_STRUCT))
    {
      ecif.rvalue = alloca(cif->rtype->size);
    }
  else
    ecif.rvalue = rvalue;
    
  
  switch (cif->abi) 
    {
#ifndef X86_WIN32
    case FFI_SYSV:
      ffi_call_SYSV(ffi_prep_args, &ecif, cif->bytes, cif->flags, ecif.rvalue,
                    fn);
      break;
#else
    case FFI_SYSV:
    case FFI_MS_CDECL:
#endif
    case FFI_STDCALL:
    case FFI_THISCALL:
    case FFI_FASTCALL:
    case FFI_PASCAL:
    case FFI_REGISTER:
      ffi_call_win32(ffi_prep_args, &ecif, cif->abi, cif->bytes, cif->flags,
                     ecif.rvalue, fn);
      break;
    default:
      FFI_ASSERT(0);
      break;
    }
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

static unsigned int 
ffi_prep_args_raw(char *stack, extended_cif *ecif)
{
  const ffi_cif *cif = ecif->cif;
  unsigned int i, passed_regs = 0;
  
  const unsigned int abi = cif->abi;
  const unsigned int max_regs = (abi == FFI_THISCALL) ? 1
                              : (abi == FFI_FASTCALL) ? 2
                              : (abi == FFI_REGISTER) ? 3
                              : 0;

  if (cif->flags == FFI_TYPE_STRUCT)
    ++passed_regs;
  
  for (i = 0; i < cif->nargs && passed_regs <= max_regs; i++)
    {
      if (cif->arg_types[i]->type == FFI_TYPE_FLOAT
         || cif->arg_types[i]->type == FFI_TYPE_STRUCT)
        continue;

      size_t sz = cif->arg_types[i]->size;
      if (sz == 0 || sz > FFI_SIZEOF_ARG)
        continue;

      ++passed_regs;
    }

  memcpy (stack, ecif->avalue, cif->bytes);
  return passed_regs;
}

/* we borrow this routine from libffi (it must be changed, though, to
 * actually call the function passed in the first argument.  as of
 * libffi-1.20, this is not the case.)
 */

void
ffi_raw_call(ffi_cif *cif, void (*fn)(void), void *rvalue, ffi_raw *fake_avalue)
{
  extended_cif ecif;
  void **avalue = (void **)fake_avalue;

  ecif.cif = cif;
  ecif.avalue = avalue;
  
  /* If the return value is a struct and we don't have a return */
  /* value address then we need to make one                     */

  if (rvalue == NULL
      && (cif->flags == FFI_TYPE_STRUCT
          || cif->flags == FFI_TYPE_MS_STRUCT))
    {
      ecif.rvalue = alloca(cif->rtype->size);
    }
  else
    ecif.rvalue = rvalue;
    
  
  switch (cif->abi) 
    {
#ifndef X86_WIN32
    case FFI_SYSV:
      ffi_call_SYSV(ffi_prep_args_raw, &ecif, cif->bytes, cif->flags,
                    ecif.rvalue, fn);
      break;
#else
    case FFI_SYSV:
    case FFI_MS_CDECL:
#endif
    case FFI_STDCALL:
    case FFI_THISCALL:
    case FFI_FASTCALL:
    case FFI_PASCAL:
    case FFI_REGISTER:
      ffi_call_win32(ffi_prep_args_raw, &ecif, cif->abi, cif->bytes, cif->flags,
                     ecif.rvalue, fn);
      break;
    default:
      FFI_ASSERT(0);
      break;
    }
}
#endif /* !FFI_NO_RAW_API */
#endif /* !__x86_64__ */

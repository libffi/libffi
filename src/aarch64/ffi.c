/* Copyright (c) 2009, 2010, 2011, 2012 ARM Ltd.

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
``Software''), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED ``AS IS'', WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.  */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <ffi.h>
#include <ffi_common.h>
#include "internal.h"

/* Force FFI_TYPE_LONGDOUBLE to be different than FFI_TYPE_DOUBLE;
   all further uses in this file will refer to the 128-bit type.  */
#if FFI_TYPE_DOUBLE != FFI_TYPE_LONGDOUBLE
# if FFI_TYPE_LONGDOUBLE != 4
#  error FFI_TYPE_LONGDOUBLE out of date
# endif
#else
# undef FFI_TYPE_LONGDOUBLE
# define FFI_TYPE_LONGDOUBLE 4
#endif

union _d
{
  UINT64 d;
  UINT32 s[2];
};

struct _v
{
  union _d d[2] __attribute__((aligned(16)));
};

struct call_context
{
  struct _v v[N_V_ARG_REG];
  UINT64 x[N_X_ARG_REG];
  UINT64 x8;
};

#if defined (__clang__) && defined (__APPLE__)
extern void sys_icache_invalidate (void *start, size_t len);
#endif

static inline void
ffi_clear_cache (void *start, void *end)
{
#if defined (__clang__) && defined (__APPLE__)
  sys_icache_invalidate (start, (char *)end - (char *)start);
#elif defined (__GNUC__)
  __builtin___clear_cache (start, end);
#else
#error "Missing builtin to flush instruction cache"
#endif
}

extern void
ffi_closure_SYSV (ffi_closure *);

/* Test for an FFI floating point representation.  */

static unsigned
is_floating_type (unsigned short type)
{
  return (type == FFI_TYPE_FLOAT || type == FFI_TYPE_DOUBLE
	  || type == FFI_TYPE_LONGDOUBLE);
}

/* A subroutine of is_hfa.  Given a structure type, return the type code
   of the first non-structure element.  Recurse for structure elements.
   Return -1 if the structure is in fact empty, i.e. no nested elements.  */

static int
is_hfa0 (const ffi_type *ty)
{
  ffi_type **elements = ty->elements;
  int i, ret = -1;

  if (elements != NULL)
    for (i = 0; elements[i]; ++i)
      {
        ret = elements[i]->type;
        if (ret == FFI_TYPE_STRUCT)
          {
            ret = is_hfa0 (elements[i]);
            if (ret < 0)
              continue;
          }
        break;
      }

  return ret;
}

/* A subroutine of is_hfa.  Given a structure type, return true if all
   of the non-structure elements are the same as CANDIDATE.  */

static int
is_hfa1 (const ffi_type *ty, int candidate)
{
  ffi_type **elements = ty->elements;
  int i;

  if (elements != NULL)
    for (i = 0; elements[i]; ++i)
      {
        int t = elements[i]->type;
        if (t == FFI_TYPE_STRUCT)
          {
            if (!is_hfa1 (elements[i], candidate))
              return 0;
          }
        else if (t != candidate)
          return 0;
      }

  return 1;
}

/* Determine if TY is an homogenous floating point aggregate (HFA).
   That is, a structure consisting of 1 to 4 members of all the same type,
   where that type is a floating point scalar.

   Returns non-zero iff TY is an HFA.  The result is an encoded value where
   bits 0-7 contain the type code, and bits 8-10 contain the element count.  */

static int
is_hfa(const ffi_type *ty)
{
  ffi_type **elements;
  int candidate, i;
  size_t size, ele_count;

  /* Quickest tests first.  */
  if (ty->type != FFI_TYPE_STRUCT)
    return 0;

  /* No HFA types are smaller than 4 bytes, or larger than 64 bytes.  */
  size = ty->size;
  if (size < 4 || size > 64)
    return 0;

  /* Find the type of the first non-structure member.  */
  elements = ty->elements;
  candidate = elements[0]->type;
  if (candidate == FFI_TYPE_STRUCT)
    {
      for (i = 0; ; ++i)
        {
          candidate = is_hfa0 (elements[i]);
          if (candidate >= 0)
            break;
        }
    }

  /* If the first member is not a floating point type, it's not an HFA.
     Also quickly re-check the size of the structure.  */
  switch (candidate)
    {
    case FFI_TYPE_FLOAT:
      ele_count = size / sizeof(float);
      if (size != ele_count * sizeof(float))
        return 0;
      break;
    case FFI_TYPE_DOUBLE:
      ele_count = size / sizeof(double);
      if (size != ele_count * sizeof(double))
        return 0;
      break;
    case FFI_TYPE_LONGDOUBLE:
      ele_count = size / sizeof(long double);
      if (size != ele_count * sizeof(long double))
        return 0;
      break;
    default:
      return 0;
    }
  if (ele_count > 4)
    return 0;

  /* Finally, make sure that all scalar elements are the same type.  */
  for (i = 0; elements[i]; ++i)
    {
      if (elements[i]->type == FFI_TYPE_STRUCT)
        {
          if (!is_hfa1 (elements[i], candidate))
            return 0;
        }
      else if (elements[i]->type != candidate)
        return 0;
    }

  /* All tests succeeded.  Encode the result.  */
  return (ele_count << 8) | candidate;
}

/* Test if an ffi_type is a candidate for passing in a register.

   This test does not check that sufficient registers of the
   appropriate class are actually available, merely that IFF
   sufficient registers are available then the argument will be passed
   in register(s).

   Note that an ffi_type that is deemed to be a register candidate
   will always be returned in registers.

   Returns 1 if a register candidate else 0.  */

static int
is_register_candidate (ffi_type *ty)
{
  switch (ty->type)
    {
    case FFI_TYPE_VOID:
      return 0;
    case FFI_TYPE_FLOAT:
    case FFI_TYPE_DOUBLE:
    case FFI_TYPE_LONGDOUBLE:
    case FFI_TYPE_UINT8:
    case FFI_TYPE_UINT16:
    case FFI_TYPE_UINT32:
    case FFI_TYPE_UINT64:
    case FFI_TYPE_POINTER:
    case FFI_TYPE_SINT8:
    case FFI_TYPE_SINT16:
    case FFI_TYPE_SINT32:
    case FFI_TYPE_INT:
    case FFI_TYPE_SINT64:
      return 1;

    case FFI_TYPE_STRUCT:
      if (is_hfa (ty))
        {
          return 1;
        }
      else if (ty->size > 16)
        {
          /* Too large. Will be replaced with a pointer to memory. The
             pointer MAY be passed in a register, but the value will
             not. This test specifically fails since the argument will
             never be passed by value in registers. */
          return 0;
        }
      else
        {
          /* Might be passed in registers depending on the number of
             registers required. */
          return (ty->size + 7) / 8 < N_X_ARG_REG;
        }
      break;

    default:
      FFI_ASSERT (0);
      break;
    }

  return 0;
}

/* Test if an ffi_type argument or result is a candidate for a vector
   register.  */

static int
is_v_register_candidate (ffi_type *ty)
{
  return is_floating_type (ty->type)
	   || (ty->type == FFI_TYPE_STRUCT && is_hfa (ty));
}

/* Representation of the procedure call argument marshalling
   state.

   The terse state variable names match the names used in the AARCH64
   PCS. */

struct arg_state
{
  unsigned ngrn;                /* Next general-purpose register number. */
  unsigned nsrn;                /* Next vector register number. */
  size_t nsaa;                  /* Next stack offset. */

#if defined (__APPLE__)
  unsigned allocating_variadic;
#endif
};

/* Initialize a procedure call argument marshalling state.  */
static void
arg_init (struct arg_state *state)
{
  state->ngrn = 0;
  state->nsrn = 0;
  state->nsaa = 0;
#if defined (__APPLE__)
  state->allocating_variadic = 0;
#endif
}

/* Allocate an aligned slot on the stack and return a pointer to it.  */
static void *
allocate_to_stack (struct arg_state *state, void *stack,
		   size_t alignment, size_t size)
{
  size_t nsaa = state->nsaa;

  /* Round up the NSAA to the larger of 8 or the natural
     alignment of the argument's type.  */
#if defined (__APPLE__)
  if (state->allocating_variadic && alignment < 8)
    alignment = 8;
#else
  if (alignment < 8)
    alignment = 8;
#endif
    
  nsaa = ALIGN (nsaa, alignment);
  state->nsaa = nsaa + size;

  return (char *)stack + nsaa;
}

static ffi_arg
extend_integer_type (void *source, int type)
{
  switch (type)
    {
    case FFI_TYPE_UINT8:
      return *(UINT8 *) source;
    case FFI_TYPE_SINT8:
      return *(SINT8 *) source;
    case FFI_TYPE_UINT16:
      return *(UINT16 *) source;
    case FFI_TYPE_SINT16:
      return *(SINT16 *) source;
    case FFI_TYPE_UINT32:
      return *(UINT32 *) source;
    case FFI_TYPE_INT:
    case FFI_TYPE_SINT32:
      return *(SINT32 *) source;
    case FFI_TYPE_UINT64:
    case FFI_TYPE_SINT64:
      return *(UINT64 *) source;
      break;
    case FFI_TYPE_POINTER:
      return *(uintptr_t *) source;
    default:
      abort();
    }
}

static void
extend_hfa_type (void *dest, void *src, int h)
{
  int n = (h >> 8);
  int t = h & 0xff;
  int f = (t - FFI_TYPE_FLOAT) * 4 + 4 - n;
  void *x0;

  asm volatile (
	"adr	%0, 0f\n"
"	add	%0, %0, %1\n"
"	br	%0\n"
"0:	ldp	s16, s17, [%3]\n"	/* S4 */
"	ldp	s18, s19, [%3, #8]\n"
"	b	4f\n"
"	ldp	s16, s17, [%3]\n"	/* S3 */
"	ldr	s18, [%3, #8]\n"
"	b	3f\n"
"	ldp	s16, s17, [%3]\n"	/* S2 */
"	b	2f\n"
"	nop\n"
"	ldr	s16, [%3]\n"		/* S1 */
"	b	1f\n"
"	nop\n"
"	ldp	d16, d17, [%3]\n"	/* D4 */
"	ldp	d18, d19, [%3, #16]\n"
"	b	4f\n"
"	ldp	d16, d17, [%3]\n"	/* D3 */
"	ldr	d18, [%3, #16]\n"
"	b	3f\n"
"	ldp	d16, d17, [%3]\n"	/* D2 */
"	b	2f\n"
"	nop\n"
"	ldr	d16, [%3]\n"		/* D1 */
"	b	1f\n"
"	nop\n"
"	ldp	q16, q17, [%3]\n"	/* Q4 */
"	ldp	q18, q19, [%3, #16]\n"
"	b	4f\n"
"	ldp	q16, q17, [%3]\n"	/* Q3 */
"	ldr	q18, [%3, #16]\n"
"	b	3f\n"
"	ldp	q16, q17, [%3]\n"	/* Q2 */
"	b	2f\n"
"	nop\n"
"	ldr	q16, [%3]\n"		/* Q1 */
"	b	1f\n"
"4:	str	q19, [%2, #48]\n"
"3:	str	q18, [%2, #32]\n"
"2:	str	q17, [%2, #16]\n"
"1:	str	q16, [%2]"
    : "=&r"(x0)
    : "r"(f * 12), "r"(dest), "r"(src)
    : "memory", "v16", "v17", "v18", "v19");
}

static void *
compress_hfa_type (void *dest, void *reg, int h)
{
  int n = h >> 8;
  switch (h & 0xff)
    {
    case FFI_TYPE_FLOAT:
      switch (n)
	{
	default:
	  if (dest == reg)
	    {
#ifdef __AARCH64EB__
	      dest += 12;
#endif
	    }
	  else
	    *(float *)dest = *(float *)reg;
	  break;
	case 2:
	  asm("ldp q16, q17, [%1]\n\t"
	      "st2 { v16.s, v17.s }[0], [%0]"
	      : : "r"(dest), "r"(reg) : "memory", "v16", "v17");
	  break;
	case 3:
	  asm("ldp q16, q17, [%1]\n\t"
	      "ldr q18, [%1, #32]\n\t"
	      "st3 { v16.s, v17.s, v18.s }[0], [%0]"
	      : : "r"(dest), "r"(reg) : "memory", "v16", "v17", "v18");
	  break;
	case 4:
	  asm("ldp q16, q17, [%1]\n\t"
	      "ldp q18, q19, [%1, #32]\n\t"
	      "st4 { v16.s, v17.s, v18.s, v19.s }[0], [%0]"
	      : : "r"(dest), "r"(reg) : "memory", "v16", "v17", "v18", "v19");
	  break;
	}
      break;

    case FFI_TYPE_DOUBLE:
      switch (n)
	{
	default:
	  if (dest == reg)
	    {
#ifdef __AARCH64EB__
	      dest += 8;
#endif
	    }
	  else
	    *(double *)dest = *(double *)reg;
	  break;
	case 2:
	  asm("ldp q16, q17, [%1]\n\t"
	      "st2 { v16.d, v17.d }[0], [%0]"
	      : : "r"(dest), "r"(reg) : "memory", "v16", "v17");
	  break;
	case 3:
	  asm("ldp q16, q17, [%1]\n\t"
	      "ldr q18, [%1, #32]\n\t"
	      "st3 { v16.d, v17.d, v18.d }[0], [%0]"
	      : : "r"(dest), "r"(reg) : "memory", "v16", "v17", "v18");
	  break;
	case 4:
	  asm("ldp q16, q17, [%1]\n\t"
	      "ldp q18, q19, [%1, #32]\n\t"
	      "st4 { v16.d, v17.d, v18.d, v19.d }[0], [%0]"
	      : : "r"(dest), "r"(reg) : "memory", "v16", "v17", "v18", "v19");
	  break;
	}
      break;

    case FFI_TYPE_LONGDOUBLE:
      if (dest != reg)
	return memcpy (dest, reg, 16 * n);
      break;

    default:
      FFI_ASSERT (0);
    }
  return dest;
}

/* Either allocate an appropriate register for the argument type, or if
   none are available, allocate a stack slot and return a pointer
   to the allocated space.  */

static void *
allocate_int_to_reg_or_stack (struct call_context *context,
			      struct arg_state *state,
			      void *stack, size_t size)
{
  if (state->ngrn < N_X_ARG_REG)
    return &context->x[state->ngrn++];

  state->ngrn = N_X_ARG_REG;
  return allocate_to_stack (state, stack, size, size);
}

ffi_status
ffi_prep_cif_machdep (ffi_cif *cif)
{
  /* Round the stack up to a multiple of the stack alignment requirement. */
  cif->bytes = ALIGN(cif->bytes, 16);

  /* Initialize our flags. We are interested if this CIF will touch a
     vector register, if so we will enable context save and load to
     those registers, otherwise not. This is intended to be friendly
     to lazy float context switching in the kernel.  */
  cif->aarch64_flags = 0;

  if (is_v_register_candidate (cif->rtype))
    {
      cif->aarch64_flags |= AARCH64_FLAG_ARG_V;
    }
  else
    {
      int i;
      for (i = 0; i < cif->nargs; i++)
        if (is_v_register_candidate (cif->arg_types[i]))
          {
            cif->aarch64_flags |= AARCH64_FLAG_ARG_V;
            break;
          }
    }

#if defined (__APPLE__)
  cif->aarch64_nfixedargs = 0;
#endif

  return FFI_OK;
}

#if defined (__APPLE__)

/* Perform Apple-specific cif processing for variadic calls */
ffi_status ffi_prep_cif_machdep_var(ffi_cif *cif,
				    unsigned int nfixedargs,
				    unsigned int ntotalargs)
{
  ffi_status status;

  status = ffi_prep_cif_machdep (cif);

  cif->aarch64_nfixedargs = nfixedargs;

  return status;
}

#endif

extern void ffi_call_SYSV (void *stack, void *frame,
			   void (*fn)(void), int flags) FFI_HIDDEN;

/* Call a function with the provided arguments and capture the return
   value.  */
void
ffi_call (ffi_cif *cif, void (*fn)(void), void *rvalue, void **avalue)
{
  struct call_context *context;
  void *stack, *frame;
  struct arg_state state;
  size_t stack_bytes;
  int i, nargs = cif->nargs;
  int h, t;
  ffi_type *rtype;

  /* Allocate consectutive stack for everything we'll need.  */
  stack_bytes = cif->bytes;
  stack = alloca (stack_bytes + 32 + sizeof(struct call_context));
  frame = stack + stack_bytes;
  context = frame + 32;

  arg_init (&state);
  for (i = 0; i < nargs; i++)
    {
      ffi_type *ty = cif->arg_types[i];
      size_t s = ty->size;
      void *a = avalue[i];

      t = ty->type;
      switch (t)
	{
	case FFI_TYPE_VOID:
	  FFI_ASSERT (0);
	  break;

	/* If the argument is a basic type the argument is allocated to an
	   appropriate register, or if none are available, to the stack.  */
	case FFI_TYPE_INT:
	case FFI_TYPE_UINT8:
	case FFI_TYPE_SINT8:
	case FFI_TYPE_UINT16:
	case FFI_TYPE_SINT16:
	case FFI_TYPE_UINT32:
	case FFI_TYPE_SINT32:
	case FFI_TYPE_UINT64:
	case FFI_TYPE_SINT64:
	case FFI_TYPE_POINTER:
	do_pointer:
	  {
	    ffi_arg ext = extend_integer_type (a, t);
	    if (state.ngrn < N_X_ARG_REG)
	      context->x[state.ngrn++] = ext;
	    else
	      {
		void *d = allocate_to_stack (&state, stack, ty->alignment, s);
		state.ngrn = N_X_ARG_REG;
		/* Note that the default abi extends each argument
		   to a full 64-bit slot, while the iOS abi allocates
		   only enough space. */
#ifdef __APPLE__
		memcpy(d, a, s);
#else
		*(ffi_arg *)d = ext;
#endif
	      }
	  }
	  break;

	case FFI_TYPE_FLOAT:
	case FFI_TYPE_DOUBLE:
	case FFI_TYPE_LONGDOUBLE:
	  /* Scalar float is a degenerate case of HFA.  */
	  h = t + 0x100;
	  goto do_hfa;

	case FFI_TYPE_STRUCT:
	  {
	    void *dest;
	    int elems;

	    h = is_hfa (ty);
	    if (h)
	      {
	    do_hfa:
		elems = h >> 8;
	        if (state.nsrn + elems <= N_V_ARG_REG)
		  {
		    dest = &context->v[state.nsrn];
		    state.nsrn += elems;
		    extend_hfa_type (dest, a, h);
		    break;
		  }
		state.nsrn = N_V_ARG_REG;
		dest = allocate_to_stack (&state, stack, ty->alignment, s);
	      }
	    else if (s > 16)
	      {
		/* If the argument is a composite type that is larger than 16
		   bytes, then the argument has been copied to memory, and
		   the argument is replaced by a pointer to the copy.  */
		a = &avalue[i];
		t = FFI_TYPE_POINTER;
		goto do_pointer;
	      }
	    else
	      {
		size_t n = (s + 7) / 8;
		if (state.ngrn + n <= N_X_ARG_REG)
		  {
		    /* If the argument is a composite type and the size in
		       double-words is not more than the number of available
		       X registers, then the argument is copied into
		       consecutive X registers.  */
		    dest = &context->x[state.ngrn];
		    state.ngrn += n;
		  }
		else
		  {
		    /* Otherwise, there are insufficient X registers. Further
		       X register allocations are prevented, the NSAA is
		       adjusted and the argument is copied to memory at the
		       adjusted NSAA.  */
		    state.ngrn = N_X_ARG_REG;
		    dest = allocate_to_stack (&state, stack, ty->alignment, s);
		  }
		}
	      memcpy (dest, a, s);
	    }
	  break;

	default:
	  abort();
	}

#if defined (__APPLE__)
      if (i + 1 == cif->aarch64_nfixedargs)
	{
	  state.ngrn = N_X_ARG_REG;
	  state.nsrn = N_V_ARG_REG;
	  state.allocating_variadic = 1;
	}
#endif
    }

  rtype = cif->rtype;
  if (is_register_candidate (rtype))
    {
      ffi_call_SYSV (stack, frame, fn, cif->aarch64_flags);

      t = rtype->type;
      switch (t)
	{
	case FFI_TYPE_INT:
	case FFI_TYPE_UINT8:
	case FFI_TYPE_SINT8:
	case FFI_TYPE_UINT16:
	case FFI_TYPE_SINT16:
	case FFI_TYPE_UINT32:
	case FFI_TYPE_SINT32:
	case FFI_TYPE_POINTER:
	case FFI_TYPE_UINT64:
	case FFI_TYPE_SINT64:
	  *(ffi_arg *)rvalue = extend_integer_type (&context->x[0], t);
	  break;

	case FFI_TYPE_FLOAT:
	case FFI_TYPE_DOUBLE:
	case FFI_TYPE_LONGDOUBLE:
	  compress_hfa_type (rvalue, &context->v[0], 0x100 + t);
	  break;

	case FFI_TYPE_STRUCT:
	  h = is_hfa (cif->rtype);
	  if (h)
	    compress_hfa_type (rvalue, &context->v[0], h);
	  else
	    {
	      FFI_ASSERT (rtype->size <= 16);
	      memcpy (rvalue, &context->x[0], rtype->size);
	    }
	  break;

	default:
	  FFI_ASSERT (0);
	  break;
	}
    }
  else
    {
      context->x8 = (uintptr_t)rvalue;
      ffi_call_SYSV (stack, frame, fn, cif->aarch64_flags);
    }
}

static unsigned char trampoline [] =
{ 0x70, 0x00, 0x00, 0x58,	/* ldr	x16, 1f	*/
  0x91, 0x00, 0x00, 0x10,	/* adr	x17, 2f	*/
  0x00, 0x02, 0x1f, 0xd6	/* br	x16	*/
};

/* Build a trampoline.  */

#define FFI_INIT_TRAMPOLINE(TRAMP,FUN,CTX,FLAGS)			\
  ({unsigned char *__tramp = (unsigned char*)(TRAMP);			\
    UINT64  __fun = (UINT64)(FUN);					\
    UINT64  __ctx = (UINT64)(CTX);					\
    UINT64  __flags = (UINT64)(FLAGS);					\
    memcpy (__tramp, trampoline, sizeof (trampoline));			\
    memcpy (__tramp + 12, &__fun, sizeof (__fun));			\
    memcpy (__tramp + 20, &__ctx, sizeof (__ctx));			\
    memcpy (__tramp + 28, &__flags, sizeof (__flags));			\
    ffi_clear_cache(__tramp, __tramp + FFI_TRAMPOLINE_SIZE);		\
  })

ffi_status
ffi_prep_closure_loc (ffi_closure* closure,
                      ffi_cif* cif,
                      void (*fun)(ffi_cif*,void*,void**,void*),
                      void *user_data,
                      void *codeloc)
{
  if (cif->abi != FFI_SYSV)
    return FFI_BAD_ABI;

  FFI_INIT_TRAMPOLINE (&closure->tramp[0], &ffi_closure_SYSV, codeloc,
		       cif->aarch64_flags);

  closure->cif  = cif;
  closure->user_data = user_data;
  closure->fun  = fun;

  return FFI_OK;
}

/* Primary handler to setup and invoke a function within a closure.

   A closure when invoked enters via the assembler wrapper
   ffi_closure_SYSV(). The wrapper allocates a call context on the
   stack, saves the interesting registers (from the perspective of
   the calling convention) into the context then passes control to
   ffi_closure_SYSV_inner() passing the saved context and a pointer to
   the stack at the point ffi_closure_SYSV() was invoked.

   On the return path the assembler wrapper will reload call context
   registers.

   ffi_closure_SYSV_inner() marshalls the call context into ffi value
   descriptors, invokes the wrapped function, then marshalls the return
   value back into the call context.  */

void FFI_HIDDEN
ffi_closure_SYSV_inner (ffi_closure *closure, struct call_context *context,
			void *stack)
{
  ffi_cif *cif = closure->cif;
  void **avalue = (void**) alloca (cif->nargs * sizeof (void*));
  void *rvalue = NULL;
  int i, h, nargs = cif->nargs;
  struct arg_state state;
  ffi_type *rtype;

  arg_init (&state);

  for (i = 0; i < nargs; i++)
    {
      ffi_type *ty = cif->arg_types[i];
      int t = ty->type;
      size_t n, s = ty->size;

      switch (t)
	{
	case FFI_TYPE_VOID:
	  FFI_ASSERT (0);
	  break;

	case FFI_TYPE_INT:
	case FFI_TYPE_UINT8:
	case FFI_TYPE_SINT8:
	case FFI_TYPE_UINT16:
	case FFI_TYPE_SINT16:
	case FFI_TYPE_UINT32:
	case FFI_TYPE_SINT32:
	case FFI_TYPE_UINT64:
	case FFI_TYPE_SINT64:
	case FFI_TYPE_POINTER:
	  avalue[i] = allocate_int_to_reg_or_stack (context, &state, stack, s);
	  break;

	case FFI_TYPE_FLOAT:
	case FFI_TYPE_DOUBLE:
	case FFI_TYPE_LONGDOUBLE:
	  /* Scalar float is a degenerate case of HFA.  */
	  h = t + 0x100;
	  goto do_hfa;

	case FFI_TYPE_STRUCT:
	  h = is_hfa (ty);
	  if (h)
	    {
	    do_hfa:
	      n = h >> 8;
	      if (state.nsrn + n <= N_V_ARG_REG)
		{
		  void *reg = &context->v[state.nsrn];
		  state.nsrn += n;

		  /* Eeek! We need a pointer to the structure, however the
		     homogeneous float elements are being passed in individual
		     registers, therefore for float and double the structure
		     is not represented as a contiguous sequence of bytes in
		     our saved register context.  We don't need the original
		     contents of the register storage, so we reformat the
		     structure into the same memory.  */
		  avalue[i] = compress_hfa_type (reg, reg, h);
		}
	      else
		{
		  state.nsrn = N_V_ARG_REG;
		  avalue[i] = allocate_to_stack (&state, stack,
						 ty->alignment, s);
		}
	    }
	  else if (s > 16)
	    {
	      /* Replace Composite type of size greater than 16 with a
		 pointer.  */
	      avalue[i] = *(void **)
		allocate_int_to_reg_or_stack (context, &state, stack,
					      sizeof (void *));
	    }
	  else
	    {
	      n = (s + 7) / 8;
	      if (state.ngrn + n <= N_X_ARG_REG)
		{
		  avalue[i] = &context->x[state.ngrn];
		  state.ngrn += n;
		}
	      else
		{
		  state.ngrn = N_X_ARG_REG;
		  avalue[i] = allocate_to_stack (&state, stack,
						 ty->alignment, s);
		}
	    }
	  break;

	default:
	  abort();
	}
    }

  /* Figure out where the return value will be passed, either in registers
     or in a memory block allocated by the caller and passed in x8.  */
  rtype = cif->rtype;
  if (is_register_candidate (rtype))
    {
      size_t s = rtype->size;
      int t;

      /* Register candidates are *always* returned in registers. */

      /* Allocate a scratchpad for the return value, we will let the
         callee scrible the result into the scratch pad then move the
         contents into the appropriate return value location for the
         call convention.  */
      rvalue = alloca (s);
      (closure->fun) (cif, rvalue, avalue, closure->user_data);

      /* Copy the return value into the call context so that it is returned
         as expected to our caller.  */
      t = rtype->type;
      switch (t)
        {
        case FFI_TYPE_VOID:
          break;

        case FFI_TYPE_INT:
        case FFI_TYPE_UINT8:
        case FFI_TYPE_UINT16:
        case FFI_TYPE_UINT32:
        case FFI_TYPE_UINT64:
        case FFI_TYPE_SINT8:
        case FFI_TYPE_SINT16:
        case FFI_TYPE_SINT32:
        case FFI_TYPE_SINT64:
        case FFI_TYPE_POINTER:
	  context->x[0] = extend_integer_type (rvalue, t);
          break;

        case FFI_TYPE_FLOAT:
        case FFI_TYPE_DOUBLE:
        case FFI_TYPE_LONGDOUBLE:
	  extend_hfa_type (&context->v[0], rvalue, 0x100 + t);
	  break;

        case FFI_TYPE_STRUCT:
	  h = is_hfa (cif->rtype);
          if (h)
	    extend_hfa_type (&context->v[0], rvalue, h);
          else
	    {
	      FFI_ASSERT (s <= 16);
              memcpy (&context->x[0], rvalue, s);
            }
          break;

        default:
          abort();
        }
    }
  else
    {
      rvalue = (void *)(uintptr_t)context->x8;
      (closure->fun) (cif, rvalue, avalue, closure->user_data);
    }
}


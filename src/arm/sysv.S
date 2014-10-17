/* -----------------------------------------------------------------------
   sysv.S - Copyright (c) 1998, 2008, 2011 Red Hat, Inc.
	    Copyright (c) 2011 Plausible Labs Cooperative, Inc.

   ARM Foreign Function Interface

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

#define LIBFFI_ASM	
#include <fficonfig.h>
#include <ffi.h>
#include <ffi_cfi.h>
#include "internal.h"

/* GCC 4.8 provides __ARM_ARCH; construct it otherwise.  */
#ifndef __ARM_ARCH
# if defined(__ARM_ARCH_7__) || defined(__ARM_ARCH_7A__) \
     || defined(__ARM_ARCH_7R__) || defined(__ARM_ARCH_7M__) \
     || defined(__ARM_ARCH_7EM__)
#  define __ARM_ARCH 7
# elif defined(__ARM_ARCH_6__) || defined(__ARM_ARCH_6J__) \
        || defined(__ARM_ARCH_6K__) || defined(__ARM_ARCH_6Z__) \
        || defined(__ARM_ARCH_6ZK__) || defined(__ARM_ARCH_6T2__) \
	|| defined(__ARM_ARCH_6M__)
#  define __ARM_ARCH 6
# elif defined(__ARM_ARCH_5__) || defined(__ARM_ARCH_5T__) \
	|| defined(__ARM_ARCH_5E__) || defined(__ARM_ARCH_5TE__) \
	|| defined(__ARM_ARCH_5TEJ__)
#  define __ARM_ARCH 5
# else
#  define __ARM_ARCH 4
# endif
#endif

/* Conditionally compile unwinder directives.  */
.macro UNWIND text:vararg
#ifdef __ARM_EABI__
	\text
#endif	
.endm
#if defined(HAVE_AS_CFI_PSEUDO_OP) && defined(__ARM_EABI__)
	.cfi_sections	.debug_frame
#endif

#define CONCAT(a, b)	CONCAT2(a, b)
#define CONCAT2(a, b)	a ## b

#ifdef __USER_LABEL_PREFIX__
# define CNAME(X)	CONCAT (__USER_LABEL_PREFIX__, X)
#else
# define CNAME(X)	X
#endif
#ifdef __ELF__
# define SIZE(X)	.size CNAME(X), . - CNAME(X)
# define TYPE(X, Y)	.type CNAME(X), Y
#else
# define SIZE(X)
# define TYPE(X, Y)
#endif

#define ARM_FUNC_START(name, gl) \
	.align	3; \
	.ifne gl; .globl CNAME(name); FFI_HIDDEN(CNAME(name)); .endif; \
	TYPE(name, %function); \
	CNAME(name):

#define ARM_FUNC_END(name) \
	SIZE(name)

/* Aid in defining a jump table with 8 bytes between entries.  */
.macro E index
	.if . - 0b - 8*\index
	.error "type table out of sync"
	.endif
.endm

	.text
	.syntax unified
	.arm

	/* We require interworking on LDM, which implies ARMv5T,
	   which implies the existance of BLX.  */
	.arch	armv5t

	/* Note that we use STC and LDC to encode VFP instructions,
	   so that we do not need ".fpu vfp", nor get that added to
	   the object file attributes.  These will not be executed
	   unless the FFI_VFP abi is used.  */

	@ r0:   stack
	@ r1:   frame
	@ r2:   fn
	@ r3:	vfp_used

ARM_FUNC_START(ffi_call_VFP, 1)
	UNWIND	.fnstart
	cfi_startproc

	cmp	r3, #3			@ load only d0 if possible
	ldcle	p11, cr0, [r0]		@ vldrle d0, [sp]
	ldcgt	p11, cr0, [r0], {16}	@ vldmgt sp, {d0-d7}
	add	r0, r0, #64		@ discard the vfp register args
	/* FALLTHRU */
ARM_FUNC_END(ffi_call_VFP)

ARM_FUNC_START(ffi_call_SYSV, 1)
	stm	r1, {fp, lr}
	mov	fp, r1

	@ This is a bit of a lie wrt the origin of the unwind info, but
	@ now we've got the usual frame pointer and two saved registers.
	UNWIND	.save {fp,lr}
	UNWIND	.setfp fp, sp
	cfi_def_cfa(fp, 8)
	cfi_rel_offset(fp, 0)
	cfi_rel_offset(lr, 4)

	mov	sp, r0		@ install the stack pointer
	mov	lr, r2		@ move the fn pointer out of the way
	ldmia	sp!, {r0-r3}	@ move first 4 parameters in registers.
	blx	lr		@ call fn

	@ Load r2 with the pointer to storage for the return value
	@ Load r3 with the return type code
	ldr	r2, [fp, #8]
	ldr	r3, [fp, #12]

	@ Deallocate the stack with the arguments.
	mov	sp, fp
	cfi_def_cfa_register(sp)

	@ Store values stored in registers.
	.align	3
	add	pc, pc, r3, lsl #3
	nop
0:
E ARM_TYPE_VFP_S
	stc	p10, cr0, [r2]		@ vstr s0, [r2]
	pop	{fp,pc}
E ARM_TYPE_VFP_D
	stc	p11, cr0, [r2]		@ vstr d0, [r2]
	pop	{fp,pc}
E ARM_TYPE_VFP_N
	stc	p11, cr0, [r2], {8}	@ vstm r2, {d0-d3}
	pop	{fp,pc}
E ARM_TYPE_INT64
	str	r1, [r2, #4]
	nop
E ARM_TYPE_INT
	str	r0, [r2]
	pop	{fp,pc}
E ARM_TYPE_VOID
	pop	{fp,pc}
	nop
E ARM_TYPE_STRUCT
	pop	{fp,pc}

	cfi_endproc
	UNWIND	.fnend
ARM_FUNC_END(ffi_call_SYSV)


/*
	unsigned int FFI_HIDDEN
	ffi_closure_inner (closure, respp, args)
	     ffi_closure *closure;
	     void **respp;
  	     void *args;
*/

ARM_FUNC_START(ffi_closure_SYSV, 1)
	UNWIND	.fnstart
	UNWIND .pad #16
	add	ip, sp, #16
	stmfd	sp!, {ip, lr}
	UNWIND .save	{r0, lr}
	add	r2, sp, #8
	UNWIND .pad #16
	sub	sp, sp, #16
	str	sp, [sp, #8]
	add	r1, sp, #8
	bl	CNAME(ffi_closure_inner)
	cmp	r0, #FFI_TYPE_INT
	beq	.Lretint

	cmp	r0, #FFI_TYPE_FLOAT
#if defined(__SOFTFP__) || defined(__ARM_EABI__)
	beq	.Lretint
#else
	beq	.Lretfloat
#endif

	cmp	r0, #FFI_TYPE_DOUBLE
#if defined(__SOFTFP__) || defined(__ARM_EABI__)
	beq	.Lretlonglong
#else
	beq	.Lretdouble
#endif

	cmp	r0, #FFI_TYPE_LONGDOUBLE
#if defined(__SOFTFP__) || defined(__ARM_EABI__)
	beq	.Lretlonglong
#else
	beq	.Lretlongdouble
#endif

	cmp	r0, #FFI_TYPE_SINT64
	beq	.Lretlonglong
.Lclosure_epilogue:
	add	sp, sp, #16
	ldmfd	sp, {sp, pc}
.Lretint:
	ldr	r0, [sp]
	b	.Lclosure_epilogue
.Lretlonglong:
	ldr	r0, [sp]
	ldr	r1, [sp, #4]
	b	.Lclosure_epilogue

#if !defined(__SOFTFP__) && !defined(__ARM_EABI__)
.Lretfloat:
	ldfs	f0, [sp]
	b	.Lclosure_epilogue
.Lretdouble:
	ldfd	f0, [sp]
	b	.Lclosure_epilogue
.Lretlongdouble:
	ldfd	f0, [sp]
	b	.Lclosure_epilogue
#endif
	UNWIND .fnend
ARM_FUNC_END(ffi_closure_SYSV)


/* Below are VFP hard-float ABI call and closure implementations.
   Add VFP FPU directive here. This is only compiled into the library
   under EABI.  */
#ifdef __ARM_EABI__
ARM_FUNC_START(ffi_closure_VFP, 1)
	UNWIND	.fnstart
	fstmfdd	sp!, {d0-d7}
	@ r0-r3, then d0-d7
	UNWIND .pad #80
	add	ip, sp, #80
	stmfd	sp!, {ip, lr}
	UNWIND .save	{r0, lr}
	add	r2, sp, #72
	add	r3, sp, #8
	UNWIND .pad #72
	sub	sp, sp, #72
	str	sp, [sp, #64]
	add	r1, sp, #64
	bl	CNAME(ffi_closure_inner)

	cmp	r0, #FFI_TYPE_INT
	beq	.Lretint_vfp

	cmp	r0, #FFI_TYPE_FLOAT
	beq	.Lretfloat_vfp

	cmp	r0, #FFI_TYPE_DOUBLE
	cmpne	r0, #FFI_TYPE_LONGDOUBLE
	beq	.Lretdouble_vfp

	cmp	r0, #FFI_TYPE_SINT64
	beq	.Lretlonglong_vfp

	cmp	r0, #FFI_TYPE_STRUCT_VFP_FLOAT
	beq	.Lretfloat_struct_vfp

	cmp	r0, #FFI_TYPE_STRUCT_VFP_DOUBLE
	beq	.Lretdouble_struct_vfp
	
.Lclosure_epilogue_vfp:
	add	sp, sp, #72
	ldmfd	sp, {sp, pc}

.Lretfloat_vfp:
	flds	s0, [sp]
	b	.Lclosure_epilogue_vfp
.Lretdouble_vfp:
	fldd	d0, [sp]
	b	.Lclosure_epilogue_vfp
.Lretint_vfp:
	ldr	r0, [sp]
	b	.Lclosure_epilogue_vfp
.Lretlonglong_vfp:
	ldmia	sp, {r0, r1}
	b	.Lclosure_epilogue_vfp
.Lretfloat_struct_vfp:
	fldmiad	sp, {d0-d1}
	b	.Lclosure_epilogue_vfp
.Lretdouble_struct_vfp:
	fldmiad	sp, {d0-d3}
	b	.Lclosure_epilogue_vfp
	UNWIND .fnend
ARM_FUNC_END(ffi_closure_VFP)
#endif

ARM_FUNC_START(ffi_arm_trampoline, 1)
	stmfd sp!, {r0-r3}
	ldr r0, [pc]
	ldr pc, [pc]
ARM_FUNC_END(ffi_arm_trampoline)

#if defined __ELF__ && defined __linux__
	.section	.note.GNU-stack,"",%progbits
#endif

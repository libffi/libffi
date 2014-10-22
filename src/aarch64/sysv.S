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

#define LIBFFI_ASM
#include <fficonfig.h>
#include <ffi.h>
#include <ffi_cfi.h>
#include "internal.h"

#ifdef HAVE_MACHINE_ASM_H
#include <machine/asm.h>
#else
#ifdef __USER_LABEL_PREFIX__
#define CONCAT1(a, b) CONCAT2(a, b)
#define CONCAT2(a, b) a ## b

/* Use the right prefix for global labels.  */
#define CNAME(x) CONCAT1 (__USER_LABEL_PREFIX__, x)
#else
#define CNAME(x) x
#endif
#endif

	.text
	.align 4

	.globl	CNAME(ffi_call_SYSV)
#ifdef __ELF__
	.type	CNAME(ffi_call_SYSV), #function
	.hidden	CNAME(ffi_call_SYSV)
#endif

/* ffi_call_SYSV
   extern void ffi_call_SYSV (void *stack, void *frame,
			      void (*fn)(void), void *rvalue, int flags);

   Therefore on entry we have:

   x0 stack
   x1 frame
   x2 fn
   x3 rvalue
   x4 flags
*/

	cfi_startproc
CNAME(ffi_call_SYSV):
	/* Use a stack frame allocated by our caller.  */
	cfi_def_cfa(x1, 32);
	stp	x29, x30, [x1]
	mov	x29, x1
	mov	sp, x0
	cfi_def_cfa_register(x29)
	cfi_rel_offset (x29, 0)
	cfi_rel_offset (x30, 8)

	mov	x9, x2			/* save fn */
	mov	x8, x3			/* install structure return */
	stp	x3, x4, [x29, #16]	/* save rvalue and flags */

	/* Load the vector argument passing registers, if necessary.  */
	tbz	w4, #AARCH64_FLAG_ARG_V_BIT, 1f
	ldp     q0, q1, [sp, #0]
	ldp     q2, q3, [sp, #32]
	ldp     q4, q5, [sp, #64]
	ldp     q6, q7, [sp, #96]
1:
	/* Load the core argument passing registers, including
	   the structure return pointer.  */
	ldp     x0, x1, [sp, #16*N_V_ARG_REG + 0]
	ldp     x2, x3, [sp, #16*N_V_ARG_REG + 16]
	ldp     x4, x5, [sp, #16*N_V_ARG_REG + 32]
	ldp     x6, x7, [sp, #16*N_V_ARG_REG + 48]

	/* Deallocate the context, leaving the stacked arguments.  */
	add	sp, sp, #CALL_CONTEXT_SIZE

	blr     x9			/* call fn */

	ldp	x3, x4, [x29, #16]	/* reload rvalue and flags */

	/* Partially deconstruct the stack frame.  */
	mov     sp, x29
	cfi_def_cfa_register (sp)
	ldp     x29, x30, [x29]

	/* Save the return value as directed.  */
	adr	x5, 0f
	and	w4, w4, #AARCH64_RET_MASK
	add	x5, x5, x4, lsl #3
	br	x5

	/* Note that each table entry is 2 insns, and thus 8 bytes.
	   For integer data, note that we're storing into ffi_arg
	   and therefore we want to extend to 64 bits; these types
	   have two consecutive entries allocated for them.  */
	.align	4
0:	ret				/* VOID */
	nop
1:	str	x0, [x3]		/* INT64 */
	ret
2:	stp	x0, x1, [x3]		/* INT128 */
	ret
3:	brk	#1000			/* UNUSED */
	ret
4:	brk	#1000			/* UNUSED */
	ret
5:	brk	#1000			/* UNUSED */
	ret
6:	brk	#1000			/* UNUSED */
	ret
7:	brk	#1000			/* UNUSED */
	ret
8:	st4	{ v0.s-v3.s }[0], [x3]	/* S4 */
	ret
9:	st3	{ v0.s-v2.s }[0], [x3]	/* S3 */
	ret
10:	stp	s0, s1, [x3]		/* S2 */
	ret
11:	str	s0, [x3]		/* S1 */
	ret
12:	st4	{ v0.d-v3.d }[0], [x3]	/* D4 */
	ret
13:	st3	{ v0.d-v2.d }[0], [x3]	/* D3 */
	ret
14:	stp	d0, d1, [x3]		/* D2 */
	ret
15:	str	d0, [x3]		/* D1 */
	ret
16:	str	q3, [x3, #48]		/* Q4 */
	nop
17:	str	q2, [x3, #32]		/* Q3 */
	nop
18:	stp	q0, q1, [x3]		/* Q2 */
	ret
19:	str	q0, [x3]		/* Q1 */
	ret
20:	uxtb	w0, w0			/* UINT8 */
	str	x0, [x3]
21:	ret				/* reserved */
	nop
22:	uxth	w0, w0			/* UINT16 */
	str	x0, [x3]
23:	ret				/* reserved */
	nop
24:	mov	w0, w0			/* UINT32 */
	str	x0, [x3]
25:	ret				/* reserved */
	nop
26:	sxtb	x0, w0			/* SINT8 */
	str	x0, [x3]
27:	ret				/* reserved */
	nop
28:	sxth	x0, w0			/* SINT16 */
	str	x0, [x3]
29:	ret				/* reserved */
	nop
30:	sxtw	x0, w0			/* SINT32 */
	str	x0, [x3]
31:	ret				/* reserved */
	nop

	cfi_endproc
#ifdef __ELF__
        .size CNAME(ffi_call_SYSV), .-CNAME(ffi_call_SYSV)
#endif

#define ffi_closure_SYSV_FS (8 * 2 + CALL_CONTEXT_SIZE)

/* ffi_closure_SYSV

   Closure invocation glue. This is the low level code invoked directly by
   the closure trampoline to setup and call a closure.

   On entry x17 points to a struct trampoline_data, x16 has been clobbered
   all other registers are preserved.

   We allocate a call context and save the argument passing registers,
   then invoked the generic C ffi_closure_SYSV_inner() function to do all
   the real work, on return we load the result passing registers back from
   the call context.

   On entry

   extern void
   ffi_closure_SYSV (struct trampoline_data *);

   struct trampoline_data
   {
        UINT64 *ffi_closure;
        UINT64 flags;
   };

   This function uses the following stack frame layout:

   ==
                saved x30(lr)
   x29(fp)->    saved x29(fp)
                saved x22
                saved x21
                ...
   sp     ->    call_context
   ==

   Voila!  */

        .text
        .align 4

        .globl	CNAME(ffi_closure_SYSV)
#ifdef __ELF__
	.type	CNAME(ffi_closure_SYSV), #function
	.hidden	CNAME(ffi_closure_SYSV)
#endif
        cfi_startproc
CNAME(ffi_closure_SYSV):
        stp     x29, x30, [sp, #-16]!
	cfi_adjust_cfa_offset (16)
        cfi_rel_offset (x29, 0)
        cfi_rel_offset (x30, 8)

        mov     x29, sp
        cfi_def_cfa_register (x29)

        sub     sp, sp, #ffi_closure_SYSV_FS

        stp     x21, x22, [x29, #-16]
        cfi_rel_offset (x21, -16)
        cfi_rel_offset (x22, -8)

        /* Load x21 with &call_context.  */
        mov     x21, sp
        /* Preserve our struct trampoline_data *  */
        mov     x22, x17

        /* Save the rest of the argument passing registers, including
	   the structure return pointer.  */
        stp     x0, x1, [x21, #16*N_V_ARG_REG + 0]
        stp     x2, x3, [x21, #16*N_V_ARG_REG + 16]
        stp     x4, x5, [x21, #16*N_V_ARG_REG + 32]
        stp     x6, x7, [x21, #16*N_V_ARG_REG + 48]
        str     x8,     [x21, #16*N_V_ARG_REG + 64]

        /* Figure out if we should touch the vector registers.  */
        ldr     x0, [x22, #8]
        tbz     x0, #AARCH64_FLAG_ARG_V_BIT, 1f

        /* Save the argument passing vector registers.  */
        stp     q0, q1, [x21, #0]
        stp     q2, q3, [x21, #32]
        stp     q4, q5, [x21, #64]
        stp     q6, q7, [x21, #96]
1:
        /* Load &ffi_closure..  */
        ldr     x0, [x22, #0]
        mov     x1, x21
        /* Compute the location of the stack at the point that the
           trampoline was called.  */
        add     x2, x29, #16

        bl      CNAME(ffi_closure_SYSV_inner)

        /* Figure out if we should touch the vector registers.  */
        ldr     x0, [x22, #8]
        tbz     x0, #AARCH64_FLAG_ARG_V_BIT, 1f

        /* Load the result passing vector registers.  */
        ldp     q0, q1, [x21, #0]
        ldp     q2, q3, [x21, #32]
1:
        /* Load the result passing core registers.  */
        ldp     x0, x1, [x21, #16*N_V_ARG_REG + 0]

        /* We are done, unwind our frame.  */
        ldp     x21, x22, [x29,  #-16]
        cfi_restore (x21)
        cfi_restore (x22)

        mov     sp, x29
        cfi_def_cfa_register (sp)

        ldp     x29, x30, [sp], #16
	cfi_adjust_cfa_offset (-16)
        cfi_restore (x29)
        cfi_restore (x30)

        ret
	cfi_endproc
#ifdef __ELF__
        .size CNAME(ffi_closure_SYSV), .-CNAME(ffi_closure_SYSV)
#endif

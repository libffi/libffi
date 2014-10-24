/* -----------------------------------------------------------------------
   v9.S - Copyright (c) 2000, 2003, 2004, 2008 Red Hat, Inc.
   
   SPARC 64-bit Foreign Function Interface 

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

#ifdef SPARC64

#define C2(X, Y)  X ## Y
#define C1(X, Y)  C2(X, Y)

#ifdef __USER_LABEL_PREFIX__
# define C(Y)	C1(__USER_LABEL_PREFIX__, Y)
#else
# define C(Y)	Y
#endif
#define L(Y)	C1(.L, Y)

.macro E index
	.align	16
	.org	2b + \index * 16
.endm

#define STACK_BIAS 2047

	.text
        .align 8
	.globl	C(ffi_call_v9)
	.type	C(ffi_call_v9),@function
	FFI_HIDDEN(C(ffi_call_v9))

C(ffi_call_v9):
	cfi_startproc
	save	%sp, %o4, %sp
	cfi_def_cfa_register(%fp)
	cfi_window_save
	
	mov	%i0, %o0			! copy cif
	add	%sp, STACK_BIAS+128+48, %o1	! load args area
	mov	%i2, %o2			! copy rvalue
	call	C(ffi_prep_args_v9)
	 mov	%i3, %o3			! copy avalue

	andcc	%o0, SPARC_FLAG_FP_ARGS, %g0	! need fp regs?
	add	%sp, 48, %sp			! deallocate prep frame
	be,pt	%xcc, 1f
	 mov	%o0, %l0			! save flags

	ldd	[%sp+STACK_BIAS+128], %f0	! load all fp arg regs
	ldd	[%sp+STACK_BIAS+128+8], %f2
	ldd	[%sp+STACK_BIAS+128+16], %f4
	ldd	[%sp+STACK_BIAS+128+24], %f6
	ldd	[%sp+STACK_BIAS+128+32], %f8
	ldd	[%sp+STACK_BIAS+128+40], %f10
	ldd	[%sp+STACK_BIAS+128+48], %f12
	ldd	[%sp+STACK_BIAS+128+56], %f14
	ldd	[%sp+STACK_BIAS+128+64], %f16
	ldd	[%sp+STACK_BIAS+128+72], %f18
	ldd	[%sp+STACK_BIAS+128+80], %f20
	ldd	[%sp+STACK_BIAS+128+88], %f22
	ldd	[%sp+STACK_BIAS+128+96], %f24
	ldd	[%sp+STACK_BIAS+128+104], %f26
	ldd	[%sp+STACK_BIAS+128+112], %f28
	ldd	[%sp+STACK_BIAS+128+120], %f30

1:	ldx	[%sp+STACK_BIAS+128], %o0	! load all int arg regs
	ldx	[%sp+STACK_BIAS+128+8], %o1
	ldx	[%sp+STACK_BIAS+128+16], %o2
	ldx	[%sp+STACK_BIAS+128+24], %o3
	ldx	[%sp+STACK_BIAS+128+32], %o4
	call	%i1
	 ldx	[%sp+STACK_BIAS+128+40], %o5

0:	call	1f		! load pc in %o7
	 and	%l0, SPARC_FLAG_RET_MASK, %l1
1:	sll	%l1, 4, %l1
	add	%o7, %l1, %o7	! o7 = 0b + ret_type*16
	jmp	%o7+(2f-0b)
	 nop

	.align	16
2:
E SPARC_RET_VOID
	return	%i7+8
	 nop
E SPARC_RET_STRUCT
	add	%sp, STACK_BIAS-64+128+48, %l2
	sub	%sp, 64, %sp
	b	8f
	 stx	%o0, [%l2]
E SPARC_RET_FLOAT
	return	%i7+8
	 st	%f0, [%o2]
E SPARC_RET_DOUBLE
	return	%i7+8
	 std	%f0, [%o2]
E SPARC_RET_UINT8
	and	%o0, 0xff, %i0
	return	%i7+8
	  stx	%o0, [%o2]
E SPARC_RET_SINT8
	sll	%o0, 24, %o0
	sra	%o0, 24, %i0
	return	%i7+8
	 stx	%o0, [%o2]
E SPARC_RET_UINT16
	sll	%o0, 16, %o0
	srl	%o0, 16, %i0
	return	%i7+8
	 stx	%o0, [%o2]
E SPARC_RET_SINT16
	sll	%o0, 16, %o0
	sra	%o0, 16, %i0
	return	%i7+8
	 stx	%o0, [%o2]
E SPARC_RET_INT64
	stx	%o0, [%i2]
	return	%i7+8
	 nop
E SPARC_RET_UINT32
	srl	%o0, 0, %i0
	return	%i7+8
	 stx	%o0, [%o2]
E SPARC_RET_SINT32
	sra	%o0, 0, %i0
	return	%i7+8
	 stx	%o0, [%o2]
E SPARC_RET_LDOUBLE
	std	%f0, [%i2]
	return	%i7+8
	 std	%f2, [%o2+8]

	! Unused entries.  Don't allow bad data to do worse things.
E 12
	unimp
E 13
	unimp
E 14
	unimp
E 15
	unimp

	! Finish the SPARC_RET_STRUCT sequence.
	.align	8
8:	stx	%o1, [%l2+8]
	stx	%o2, [%l2+16]
	stx	%o3, [%l2+24]
	std	%f0, [%l2+32]
	std	%f2, [%l2+40]
	std	%f4, [%l2+48]
	std	%f6, [%l2+56]

	! Copy the structure into place.
	ldx	[%i0+16], %o0			! load rtype from cif
	mov	0, %o1				! load off
	mov	%i2, %o2			! load dst
	mov	%l2, %o3			! load src_int
	call	C(ffi_v9_layout_struct)
	 add	%l2, 32, %o4			! load src_fp

	return	%i7+8
	 nop

	cfi_endproc
	.size	C(ffi_call_v9), . - C(ffi_call_v9)


#undef STACKFRAME
#define	STACKFRAME	 336	/* 16*8 register window +
				   6*8 args backing store +
				   20*8 locals */
#define	FP		%fp+STACK_BIAS

/* ffi_closure_v9(...)

   Receives the closure argument in %g1.   */

	.align 8
	.globl	C(ffi_closure_v9)
	.type	C(ffi_closure_v9),@function
	FFI_HIDDEN(C(ffi_closure_v9))

C(ffi_closure_v9):
	cfi_startproc
	save	%sp, -STACKFRAME, %sp
	cfi_def_cfa_register(%fp)
	cfi_window_save

	! Store all of the potential argument registers in va_list format.
	stx	%i0, [FP+128+0]
	stx	%i1, [FP+128+8]
	stx	%i2, [FP+128+16]
	stx	%i3, [FP+128+24]
	stx	%i4, [FP+128+32]
	stx	%i5, [FP+128+40]

	! Store possible floating point argument registers too.
	std	%f0,  [FP-128]
	std	%f2,  [FP-120]
	std	%f4,  [FP-112]
	std	%f6,  [FP-104]
	std	%f8,  [FP-96]
	std	%f10, [FP-88]
	std     %f12, [FP-80]
	std     %f14, [FP-72]
	std     %f16, [FP-64]
	std     %f18, [FP-56]
	std     %f20, [FP-48]
	std     %f22, [FP-40]
	std     %f24, [FP-32]
	std     %f26, [FP-24]
	std     %f28, [FP-16]
	std     %f30, [FP-8]

	! Call ffi_closure_sparc_inner to do the bulk of the work.
	mov	%g1, %o0
	add	%fp, STACK_BIAS-160, %o1
	add	%fp, STACK_BIAS+128, %o2
	call	C(ffi_closure_sparc_inner_v9)
	 add	%fp, STACK_BIAS-128, %o3

0:	call	1f		! load pc in %o7
	 and	%o0, SPARC_FLAG_RET_MASK, %o0
1:	sll	%o0, 4, %o0	! o2 = i2 * 16
	add	%o7, %o0, %o7	! o7 = 0b + i2*16
	jmp	%o7+(2f-0b)
	 nop

	! Note that we cannot load the data in the delay slot of
	! the return insn because the data is in the stack frame
	! that is deallocated by the return.
	.align	16
2:
E SPARC_RET_VOID
	return	%i7+8
	 nop
E SPARC_RET_STRUCT
	ldx	[FP-160], %i0
	ldd	[FP-160], %f0
	b	8f
	 ldx	[FP-152], %i1
E SPARC_RET_FLOAT
	ld	[FP-160], %f0
	return	%i7+8
	 nop
E SPARC_RET_DOUBLE
	ldd	[FP-160], %f0
	return	%i7+8
	 nop
E SPARC_RET_UINT8
	ldub	[FP-160+7], %i0
	return	%i7+8
	 nop
E SPARC_RET_SINT8
	ldsb	[FP-160+7], %i0
	return	%i7+8
	 nop
E SPARC_RET_UINT16
	lduh	[FP-160+6], %i0
	return	%i7+8
	 nop
E SPARC_RET_SINT16
	ldsh	[FP-160+6], %i0
	return	%i7+8
	 nop
E SPARC_RET_INT64
	ldx	[FP-160], %i0
	return	%i7+8
	 nop
E SPARC_RET_UINT32
	lduw	[FP-160+4], %i0
	return	%i7+8
	 nop
E SPARC_RET_SINT32
	ldsw	[FP-160+4], %i0
	return	%i7+8
	 nop
E SPARC_RET_LDOUBLE
	ldd	[FP-160], %f0
	ldd	[FP-160+8], %f2
	return	%i7+8
	 nop

	! Unused entries.  Don't allow bad data to do worse things.
E 12
	unimp
E 13
	unimp
E 14
	unimp
E 15
	unimp

	! Finish the SPARC_RET_STRUCT sequence.
	.align	8
8:	ldd	[FP-152], %f2
	ldx	[FP-144], %i2
	ldd	[FP-144], %f4
	ldx	[FP-136], %i3
	ldd	[FP-136], %f6
	return	%i7+8
	 nop

	cfi_endproc
	.size	C(ffi_closure_v9), . - C(ffi_closure_v9)
#endif /* SPARC64 */
#ifdef __linux__
	.section	.note.GNU-stack,"",@progbits
#endif

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

#ifdef SPARC64
/* Only compile this in for 64bit builds, because otherwise the object file
   will have inproper architecture due to used instructions.  */

#define C2(X, Y)  X ## Y
#define C1(X, Y)  C2(X, Y)

#ifdef __USER_LABEL_PREFIX__
# define C(Y)	C1(__USER_LABEL_PREFIX__, Y)
#else
# define C(Y)	Y
#endif
#define L(Y)	C1(.L, Y)


#define STACKFRAME 176		/* Minimum stack framesize for SPARC 64-bit */
#define STACK_BIAS 2047
#define ARGS (128)		/* Offset of register area in frame */

.text
        .align 8
	.globl	C(ffi_call_v9)
	.type	C(ffi_call_v9),@function
	FFI_HIDDEN(C(ffi_call_v9))

C(ffi_call_v9):
	cfi_startproc
	save	%sp, -STACKFRAME, %sp
	cfi_def_cfa_register(%fp)
	cfi_window_save
	
	sub	%sp, %i2, %sp	! alloca() space in stack for frame to set up
	add	%sp, STACKFRAME+STACK_BIAS, %l0	! %l0 has start of 
						! frame to set up

	mov	%l0, %o0	! call routine to set up frame
	call	%i0
	 mov	%i1, %o1	! (delay)
	brz,pt	%o0, 1f
	 ldx	[%l0+ARGS], %o0	! call foreign function

	ldd	[%l0+ARGS], %f0
	ldd	[%l0+ARGS+8], %f2
	ldd	[%l0+ARGS+16], %f4
	ldd	[%l0+ARGS+24], %f6
	ldd	[%l0+ARGS+32], %f8
	ldd	[%l0+ARGS+40], %f10
	ldd	[%l0+ARGS+48], %f12
	ldd	[%l0+ARGS+56], %f14
	ldd	[%l0+ARGS+64], %f16
	ldd	[%l0+ARGS+72], %f18
	ldd	[%l0+ARGS+80], %f20
	ldd	[%l0+ARGS+88], %f22
	ldd	[%l0+ARGS+96], %f24
	ldd	[%l0+ARGS+104], %f26
	ldd	[%l0+ARGS+112], %f28
	ldd	[%l0+ARGS+120], %f30

1:	ldx	[%l0+ARGS+8], %o1
	ldx	[%l0+ARGS+16], %o2
	ldx	[%l0+ARGS+24], %o3
	ldx	[%l0+ARGS+32], %o4
	ldx	[%l0+ARGS+40], %o5
	call	%i5
	 sub	%l0, STACK_BIAS, %sp	! (delay) switch to frame

	! If the return value pointer is NULL, assume no return value.
	brz,pn	%i4, L(done)
	 nop

	cmp	%i3, FFI_TYPE_INT
	be,a,pt	%icc, L(done)
	 stx	%o0, [%i4+0]	! (delay)

	cmp	%i3, FFI_TYPE_FLOAT
	be,a,pn	%icc, L(done)
	 st	%f0, [%i4+0]	! (delay)

	cmp	%i3, FFI_TYPE_DOUBLE
	be,a,pn	%icc, L(done)
	 std	%f0, [%i4+0]	! (delay)

	cmp	%i3, FFI_TYPE_STRUCT
	be,pn	%icc, L(dostruct)

	cmp	%i3, FFI_TYPE_LONGDOUBLE
	bne,pt	%icc, L(done)
	 nop
	std	%f0, [%i4+0]
	std	%f2, [%i4+8]

L(done):
	ret
	 restore

L(dostruct):
	/* This will not work correctly for unions. */
	stx	%o0, [%i4+0]
	stx	%o1, [%i4+8]
	stx	%o2, [%i4+16]
	stx	%o3, [%i4+24]
	std	%f0, [%i4+32]
	std	%f2, [%i4+40]
	std	%f4, [%i4+48]
	std	%f6, [%i4+56]
	ret
	 restore

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

	! Load up the return value in the proper type.
	! See ffi_prep_cif_machdep for the list of cases.
	cmp	%o0, FFI_TYPE_VOID
	be,pn	%icc, L(done1)

	cmp	%o0, FFI_TYPE_INT
	be,pn	%icc, L(integer)

	cmp	%o0, FFI_TYPE_FLOAT
	be,a,pn	%icc, L(done1)
	 ld	[FP-160], %f0

	cmp	%o0, FFI_TYPE_DOUBLE
	be,a,pn	%icc, L(done1)
	 ldd	[FP-160], %f0

#if FFI_TYPE_LONGDOUBLE != FFI_TYPE_DOUBLE
	cmp	%o0, FFI_TYPE_LONGDOUBLE
	be,a,pn	%icc, L(longdouble1)
	 ldd	[FP-160], %f0
#endif

	! FFI_TYPE_STRUCT
	ldx	[FP-152], %i1
	ldx	[FP-144], %i2
	ldx	[FP-136], %i3
	ldd	[FP-160], %f0
	ldd	[FP-152], %f2
	ldd	[FP-144], %f4
	ldd	[FP-136], %f6

L(integer):
	ldx	[FP-160], %i0

L(done1):
	ret
	 restore

#if FFI_TYPE_LONGDOUBLE != FFI_TYPE_DOUBLE
L(longdouble1):
	ldd	[FP-152], %f2
	ret
	 restore
#endif

	cfi_endproc
	.size	C(ffi_closure_v9), . - C(ffi_closure_v9)

#endif /* SPARC64 */
#ifdef __linux__
	.section	.note.GNU-stack,"",@progbits
#endif

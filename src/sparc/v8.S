/* -----------------------------------------------------------------------
   v8.S - Copyright (c) 2013  The Written Word, Inc.
	  Copyright (c) 1996, 1997, 2003, 2004, 2008  Red Hat, Inc.
   
   SPARC Foreign Function Interface 

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

#define STACKFRAME 96		/* Minimum stack framesize for SPARC */
#define ARGS (64+4)		/* Offset of register area in frame */

#define C2(X, Y)  X ## Y
#define C1(X, Y)  C2(X, Y)

#ifdef __USER_LABEL_PREFIX__
# define C(Y)	C1(__USER_LABEL_PREFIX__, Y)
#else
# define C(Y)	Y
#endif
#define L(Y)	C1(.L, Y)

	.text

#ifndef __GNUC__	
        .align 8
	.globl	C(ffi_flush_icache)
	.type	C(ffi_flush_icache),@function
	FFI_HIDDEN(C(ffi_flush_icache))

C(ffi_flush_icache):
	cfi_startproc
        add %o0, %o1, %o2
#ifdef SPARC64	
1:	flush %o0
#else	
1:	iflush %o0
#endif
	add %o0, 8, %o0
	cmp %o0, %o2
	blt 1b
	nop
	nop
	nop
	nop
	nop
	retl
	nop
	cfi_endproc
	.size	C(ffi_flush_icache), . - C(ffi_flush_icache)
#endif

        .align 8
	.globl	C(ffi_call_v8)
	.type	C(ffi_call_v8),@function
	FFI_HIDDEN(C(ffi_call_v8))
	
C(ffi_call_v8):
	cfi_startproc
	save	%sp, -STACKFRAME, %sp
	cfi_def_cfa_register(%fp)
	cfi_window_save
	
	sub	%sp, %i2, %sp	! alloca() space in stack for frame to set up
	add	%sp, STACKFRAME, %l0	! %l0 has start of 
					! frame to set up

	mov	%l0, %o0	! call routine to set up frame
	call	%i0
	mov	%i1, %o1	! (delay)

	ld	[%l0+ARGS], %o0	! call foreign function
	ld	[%l0+ARGS+4], %o1
	ld	[%l0+ARGS+8], %o2
	ld	[%l0+ARGS+12], %o3
	ld	[%l0+ARGS+16], %o4
	ld	[%l0+ARGS+20], %o5
	call	%i5
	mov	%l0, %sp	! (delay) switch to frame
	nop			! STRUCT returning functions skip 12 instead of 8 bytes

	! If the return value pointer is NULL, assume no return value.
	tst	%i4
	bz	L(done)
	nop

	cmp	%i3, FFI_TYPE_INT
	be,a	L(done)
	st	%o0, [%i4]	! (delay)

	cmp	%i3, FFI_TYPE_FLOAT
	be,a	L(done)
	st	%f0, [%i4+0]	! (delay)

	cmp	%i3, FFI_TYPE_DOUBLE
	be,a	L(double)
	st	%f0, [%i4+0]	! (delay)

	cmp	%i3, FFI_TYPE_SINT8
	be,a	L(sint8)
	sll	%o0, 24, %o0	! (delay)

	cmp	%i3, FFI_TYPE_UINT8
	be,a	L(uint8)
	sll	%o0, 24, %o0	! (delay)

	cmp	%i3, FFI_TYPE_SINT16
	be,a	L(sint16)
	sll	%o0, 16, %o0	! (delay)

	cmp	%i3, FFI_TYPE_UINT16
	be,a	L(uint16)
	sll	%o0, 16, %o0	! (delay)

	cmp	%i3, FFI_TYPE_SINT64
	be,a	L(longlong)
	st	%o0, [%i4+0]	! (delay)

L(done):
	ret
	restore

L(double):
	st	%f1, [%i4+4]
	ret
	restore

L(sint8):
	sra	%o0, 24, %o0
	st	%o0, [%i4+0]
	ret
	restore

L(uint8):
	srl	%o0, 24, %o0
	st	%o0, [%i4+0]
	ret
	restore

L(sint16):
	sra	%o0, 16, %o0
	st	%o0, [%i4+0]
	ret
	restore

L(uint16):
	srl	%o0, 16, %o0
	st	%o0, [%i4+0]
	ret
	restore

L(longlong):
	st	%o1, [%i4+4]
	ret
	restore
	cfi_endproc

	.size	C(ffi_call_v8),. - C(ffi_call_v8)


#undef STACKFRAME
#define	STACKFRAME	104	/* 16*4 register window +
				   1*4 struct return +	
				   6*4 args backing store +
				   3*4 locals */

/* ffi_closure_v8(...)

   Receives the closure argument in %g2.   */

	.align 8
	.globl	C(ffi_closure_v8)
	.type	C(ffi_closure_v8),@function
	FFI_HIDDEN(C(ffi_closure_v8))

C(ffi_closure_v8):
#ifdef HAVE_AS_REGISTER_PSEUDO_OP
		.register	%g2, #scratch
#endif
	cfi_startproc
	! Reserve frame space for all arguments in case
	! we need to align them on a 8-byte boundary.
	ld	[%g2+FFI_TRAMPOLINE_SIZE], %g1
	ld	[%g1+4], %g1
	sll	%g1, 3, %g1
	add	%g1, STACKFRAME, %g1
	! %g1 == STACKFRAME + 8*nargs
	neg	%g1
	save	%sp, %g1, %sp
	cfi_def_cfa_register(%fp)
	cfi_window_save

	! Store all of the potential argument registers in va_list format.
	st	%i0, [%fp+68+0]
	st	%i1, [%fp+68+4]
	st	%i2, [%fp+68+8]
	st	%i3, [%fp+68+12]
	st	%i4, [%fp+68+16]
	st	%i5, [%fp+68+20]

	! Call ffi_closure_sparc_inner to do the bulk of the work.
	mov	%g2, %o0
	add	%fp, -8, %o1
	add	%fp,  64, %o2
	call	ffi_closure_sparc_inner_v8
	 add	%fp, -16, %o3

	! Load up the return value in the proper type.
	! See ffi_prep_cif_machdep for the list of cases.
	cmp	%o0, FFI_TYPE_VOID
	be	L(done1)

	cmp	%o0, FFI_TYPE_INT
	be	L(done1)
	 ld	[%fp-8], %i0

	cmp	%o0, FFI_TYPE_FLOAT
	be,a	L(done1)
	 ld	[%fp-8], %f0

	cmp	%o0, FFI_TYPE_DOUBLE
	be,a	L(done1)
	 ldd	[%fp-8], %f0

#if FFI_TYPE_LONGDOUBLE != FFI_TYPE_DOUBLE
	cmp	%o0, FFI_TYPE_LONGDOUBLE
	be	L(done2)
#endif

	cmp	%o0, FFI_TYPE_STRUCT
	be	L(done2)

	cmp	%o0, FFI_TYPE_SINT64
	be,a	L(done1)
	 ldd	[%fp-8], %i0

	cmp	%o0, FFI_TYPE_UINT64
	be,a	L(done1)
	 ldd	[%fp-8], %i0

	ld	[%fp-8], %i0
L(done1):
	jmp	%i7+8
	 restore
L(done2):
	! Skip 'unimp'.
	jmp	%i7+12
	 restore

	cfi_endproc
	.size	C(ffi_closure_v8), . - C(ffi_closure_v8)

#if defined __ELF__ && defined __linux__
	.section	.note.GNU-stack,"",@progbits
#endif

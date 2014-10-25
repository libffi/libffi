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
#include "internal.h"

#ifndef SPARC64

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
1:	iflush %o0
	iflush %o+8
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

.macro E index
	.align	16
	.org	2b + \index * 16
.endm

        .align 8
	.globl	C(ffi_call_v8)
	.type	C(ffi_call_v8),@function
	FFI_HIDDEN(C(ffi_call_v8))
	
C(ffi_call_v8):
	cfi_startproc
	! Allocate a stack frame sized by ffi_call.
	save	%sp, %o4, %sp
	cfi_def_cfa_register(%fp)
	cfi_window_save

	mov	%i0, %o0		! copy cif
	add	%sp, 64+32, %o1		! load args area
	mov	%i2, %o2		! copy rvalue
	call	C(ffi_prep_args_v8)
	 mov	%i3, %o3		! copy avalue

	add	%sp, 32, %sp		! deallocate prep frame
	and	%o0, SPARC_FLAG_RET_MASK, %l0	! save return type
	ld	[%sp+64+4], %o0		! load all argument registers
	ld	[%sp+64+8], %o1
	ld	[%sp+64+12], %o2
	ld	[%sp+64+16], %o3
	cmp	%l0, SPARC_RET_STRUCT	! struct return needs an unimp 4
	ld	[%sp+64+20], %o4
	be	8f
	 ld	[%sp+64+24], %o5

	! Call foreign function
	call	%i1
	 nop

0:	call	1f		! load pc in %o7
	 sll	%l0, 4, %l0
1:	add	%o7, %l0, %o7	! o7 = 0b + ret_type*16
	jmp	%o7+(2f-0b)
	 nop

	! Note that each entry is 4 insns, enforced by the E macro.
	.align	16
2:
E SPARC_RET_VOID
	ret
	 restore
E SPARC_RET_STRUCT
	unimp
E SPARC_RET_UINT8
	and	%o0, 0xff, %o0
	st	%o0, [%i2]
	ret
	 restore
E SPARC_RET_SINT8
	sll	%o0, 24, %o0
	b	7f
	 sra	%o0, 24, %o0
E SPARC_RET_UINT16
	sll	%o0, 16, %o0
	b	7f
	 srl	%o0, 16, %o0
E SPARC_RET_SINT16
	sll	%o0, 16, %o0
	b	7f
	 sra	%o0, 16, %o0
E SPARC_RET_UINT32
7:	st	%o0, [%i2]
	ret
	 restore
E SPARC_RET_SINT32
	unimp
E SPARC_RET_INT64
	std	%o0, [%i2]
	ret
	 restore
E SPARC_RET_INT128
	unimp
E SPARC_RET_F_8
	unimp
E SPARC_RET_F_6
	unimp
E SPARC_RET_F_4
	unimp
E SPARC_RET_F_2
	std	%f0, [%i2]
	ret
	 restore
E SPARC_RET_F_3
	unimp
E SPARC_RET_F_1
	st	%f0, [%i2]
	ret
	 restore

	! Struct returning functions expect and skip the unimp here.
	.align	8
8:	call	%i1
	 nop
	unimp	4
	ret
	 restore

	cfi_endproc
	.size	C(ffi_call_v8),. - C(ffi_call_v8)


#undef STACKFRAME
#define	STACKFRAME	104	/* 16*4 register window +
				   1*4 struct return +	
				   6*4 args backing store +
				   2*4 return storage +
				   1*4 alignment */

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
	save	%sp, -STACKFRAME, %sp
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
	call	ffi_closure_sparc_inner_v8
	 add	%fp,  64, %o2

0:	call	1f
	 and	%o0, SPARC_FLAG_RET_MASK, %o0
1:	sll	%o0, 4, %o0	! o0 = o0 * 16
	add	%o7, %o0, %o7	! o7 = 0b + o0*16
	jmp	%o7+(2f-0b)
	 nop

	! Note that each entry is 4 insns, enforced by the E macro.
	.align	16
2:
E SPARC_RET_VOID
	ret
	 restore
E SPARC_RET_STRUCT
	jmp	%i7+12
	 restore
E SPARC_RET_UINT8
	ldub	[%fp-8+3], %i0
	ret
	 restore
E SPARC_RET_SINT8
	ldsb	[%fp-8+3], %i0
	ret
	 restore
E SPARC_RET_UINT16
	lduh	[%fp-8+2], %i0
	ret
	 restore
E SPARC_RET_SINT16
	ldsh	[%fp-8+2], %i0
	ret
	 restore
E SPARC_RET_UINT32
	ld	[%fp-8], %i0
	ret
	 restore
E SPARC_RET_SINT32
	unimp
E SPARC_RET_INT64
	ldd	[%fp-8], %i0
	ret
	 restore
E SPARC_RET_INT128
	unimp
E SPARC_RET_F_8
	unimp
E SPARC_RET_F_6
	unimp
E SPARC_RET_F_4
	unimp
E SPARC_RET_F_2
	ldd	[%fp-8], %f0
	ret
	 restore
E SPARC_RET_F_3
	unimp
E SPARC_RET_F_1
	ld	[%fp-8], %f0
	ret
	 restore

	cfi_endproc
	.size	C(ffi_closure_v8), . - C(ffi_closure_v8)
#endif /* !SPARC64 */
#if defined __ELF__ && defined __linux__
	.section	.note.GNU-stack,"",@progbits
#endif

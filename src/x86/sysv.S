/* -----------------------------------------------------------------------
   sysv.S - Copyright (c) 2013  The Written Word, Inc.
	  - Copyright (c) 1996,1998,2001-2003,2005,2008,2010  Red Hat, Inc.
   
   X86 Foreign Function Interface 

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

#define LIBFFI_ASM	
#include <fficonfig.h>
#include <ffi.h>
#include <ffi_cfi.h>
#include "internal.h"

#define C2(X, Y)  X ## Y
#define C1(X, Y)  C2(X, Y)
#ifdef __USER_LABEL_PREFIX__
# define C(X)     C1(__USER_LABEL_PREFIX__, X)
#else
# define C(X)     X
#endif

#ifdef __ELF__
# define ENDF(X)  .type	X,@function; .size X, . - X
#else
# define ENDF(X)
#endif

/* This macro allows the safe creation of jump tables without an
   actual table.  The entry points into the table are all 8 bytes.
   The use of ORG asserts that we're at the correct location.  */
#define E(X)      .align 8; .org 0b + X * 8

	.text
	.align	16
	.globl	C(ffi_call_i386)
	FFI_HIDDEN(C(ffi_call_i386))

/* This is declared as

   void ffi_call_i386(struct ffi_call_frame *frame, char *argp)
        __attribute__((fastcall));

   This the arguments are present in

        ecx: frame
        edx: argp
*/

C(ffi_call_i386):
	cfi_startproc
	movl	(%esp), %eax		/* move the return address */
	movl	%ebp, (%ecx)		/* store %ebp into local frame */
	movl	%eax, 4(%ecx)		/* store retaddr into local frame */

	/* New stack frame based off ebp.  This is a itty bit of unwind
	   trickery in that the CFA *has* changed.  There is no easy way
	   to describe it correctly on entry to the function.  Fortunately,
	   it doesn't matter too much since at all points we can correctly
	   unwind back to ffi_call.  Note that the location to which we
	   moved the return address is (the new) CFA-4, so from the
	   perspective of the unwind info, it hasn't moved.  */
	movl	%ecx, %ebp
	cfi_def_cfa(%ebp, 8)
	cfi_rel_offset(%ebp, 0)

	movl	%edx, %esp		/* set outgoing argument stack */
	movl	20+R_EAX*4(%ebp), %eax	/* set register arguments */
	movl	20+R_EDX*4(%ebp), %edx
	movl	20+R_ECX*4(%ebp), %ecx

	call	*8(%ebp)

	movl	12(%ebp), %ecx		/* load return type code */
	movl	%ebx, 8(%ebp)		/* preserve %ebx */
	cfi_rel_offset(%ebp, 8)

	andl	$X86_RET_TYPE_MASK, %ecx
#ifdef __PIC__
	call	__x86.get_pc_thunk.bx
1:	leal	0f-1b(%ebx, %ecx, 8), %ebx
#else
	leal	0f(,%ecx, 8), %ebx
#endif
	movl	16(%ebp), %ecx		/* load result address */
	jmp	*%ebx

	.align	8
0:
E(X86_RET_FLOAT)
	fstps	(%ecx)
	jmp	9f
E(X86_RET_DOUBLE)
	fstpl	(%ecx)
	jmp	9f
E(X86_RET_LDOUBLE)
	fstpt	(%ecx)
	jmp	9f
E(X86_RET_SINT8)
	movsbl	%al, %eax
	mov	%eax, (%ecx)
	jmp	9f
E(X86_RET_SINT16)
	movswl	%ax, %eax
	mov	%eax, (%ecx)
	jmp	9f
E(X86_RET_UINT8)
	movzbl	%al, %eax
	mov	%eax, (%ecx)
	jmp	9f
E(X86_RET_UINT16)
	movzwl	%ax, %eax
	mov	%eax, (%ecx)
	jmp	9f
E(X86_RET_INT64)
	movl	%edx, 4(%ecx)
	/* fallthru */
E(X86_RET_INT32)
	movl	%eax, (%ecx)
	/* fallthru */
E(X86_RET_VOID)
9:	movl	8(%ebp), %ebx
	movl	%ebp, %esp
	popl	%ebp
	cfi_remember_state
	cfi_def_cfa(%esp, 4)
	cfi_restore(%ebx)
	cfi_restore(%ebp)
	ret
	cfi_restore_state

E(X86_RET_STRUCTPOP)
	jmp	9b
E(X86_RET_STRUCTARG)
	jmp	9b
E(X86_RET_STRUCT_1B)
	movb	%al, (%ecx)
	jmp	9b
E(X86_RET_STRUCT_2B)
	movw	%ax, (%ecx)
	jmp	9b

	/* Fill out the table so that bad values are predictable.  */
E(X86_RET_UNUSED14)
	ud2
E(X86_RET_UNUSED15)
	ud2

	cfi_endproc
ENDF(C(ffi_call_i386))

	.align	4
FFI_HIDDEN (ffi_closure_SYSV)
.globl ffi_closure_SYSV
	.type	ffi_closure_SYSV, @function

ffi_closure_SYSV:
	cfi_startproc
	pushl	%ebp
	cfi_adjust_cfa_offset(4)
	cfi_rel_offset(%ebp, 0)
	movl	%esp, %ebp
	cfi_def_cfa_register(%ebp)
	subl	$40, %esp
	leal	-24(%ebp), %edx
	movl	%edx, -12(%ebp)	/* resp */
	leal	8(%ebp), %edx
#ifdef __SUNPRO_C
	/* The SUNPRO compiler doesn't support GCC's regparm function
  	   attribute, so we have to pass all three arguments to
	   ffi_closure_SYSV_inner on the stack.  */
	movl	%edx, 8(%esp)	/* args = __builtin_dwarf_cfa () */
	leal	-12(%ebp), %edx
	movl	%edx, 4(%esp)	/* &resp */
	movl    %eax, (%esp)    /* closure */
#else
	movl	%edx, 4(%esp)	/* args = __builtin_dwarf_cfa () */
	leal	-12(%ebp), %edx
	movl	%edx, (%esp)	/* &resp */
#endif
#if defined HAVE_HIDDEN_VISIBILITY_ATTRIBUTE || !defined __PIC__
	call	ffi_closure_SYSV_inner
#else
	movl	%ebx, 8(%esp)
	cfi_offset(%ebx, -40)
	call	1f
1:	popl	%ebx
	addl	$_GLOBAL_OFFSET_TABLE_+[.-1b], %ebx
	call	ffi_closure_SYSV_inner@PLT
	movl	8(%esp), %ebx
	cfi_restore(%ebx)
#endif
	movl	-12(%ebp), %ecx
	cmpl	$FFI_TYPE_INT, %eax
	je	.Lcls_retint

	/* Handle FFI_TYPE_UINT8, FFI_TYPE_SINT8, FFI_TYPE_UINT16,
	   FFI_TYPE_SINT16, FFI_TYPE_UINT32, FFI_TYPE_SINT32.  */
	cmpl	$FFI_TYPE_UINT64, %eax
	jge	0f
	cmpl	$FFI_TYPE_UINT8, %eax
	jge	.Lcls_retint
	
0:	cmpl	$FFI_TYPE_FLOAT, %eax
	je	.Lcls_retfloat
	cmpl	$FFI_TYPE_DOUBLE, %eax
	je	.Lcls_retdouble
	cmpl	$FFI_TYPE_LONGDOUBLE, %eax
	je	.Lcls_retldouble
	cmpl	$FFI_TYPE_SINT64, %eax
	je	.Lcls_retllong
	cmpl	$FFI_TYPE_STRUCT, %eax
	je	.Lcls_retstruct
.Lcls_epilogue:
	movl	%ebp, %esp
	popl	%ebp
	ret
.Lcls_retint:
	movl	(%ecx), %eax
	jmp	.Lcls_epilogue
.Lcls_retfloat:
	flds	(%ecx)
	jmp	.Lcls_epilogue
.Lcls_retdouble:
	fldl	(%ecx)
	jmp	.Lcls_epilogue
.Lcls_retldouble:
	fldt	(%ecx)
	jmp	.Lcls_epilogue
.Lcls_retllong:
	movl	(%ecx), %eax
	movl	4(%ecx), %edx
	jmp	.Lcls_epilogue
.Lcls_retstruct:
	movl	%ebp, %esp
	popl	%ebp
	ret	$4
	cfi_endproc
	.size	ffi_closure_SYSV, .-ffi_closure_SYSV

#if !FFI_NO_RAW_API

/* Precalculate for e.g. the Solaris 10/x86 assembler.  */
#if FFI_TRAMPOLINE_SIZE == 10
#define RAW_CLOSURE_CIF_OFFSET 12
#define RAW_CLOSURE_FUN_OFFSET 16
#define RAW_CLOSURE_USER_DATA_OFFSET 20
#elif FFI_TRAMPOLINE_SIZE == 24
#define RAW_CLOSURE_CIF_OFFSET 24
#define RAW_CLOSURE_FUN_OFFSET 28
#define RAW_CLOSURE_USER_DATA_OFFSET 32
#else
#define RAW_CLOSURE_CIF_OFFSET ((FFI_TRAMPOLINE_SIZE + 3) & ~3)
#define RAW_CLOSURE_FUN_OFFSET (RAW_CLOSURE_CIF_OFFSET + 4)
#define RAW_CLOSURE_USER_DATA_OFFSET (RAW_CLOSURE_FUN_OFFSET + 4)
#endif
#define CIF_FLAGS_OFFSET 20

	.align	4
FFI_HIDDEN (ffi_closure_raw_SYSV)
.globl ffi_closure_raw_SYSV
	.type	ffi_closure_raw_SYSV, @function

ffi_closure_raw_SYSV:
	cfi_startproc
	pushl	%ebp
	cfi_adjust_cfa_offset(4)
	cfi_rel_offset(%ebp, 0)
	movl	%esp, %ebp
	cfi_def_cfa_register(%ebp)
	pushl	%esi
	cfi_offset(%esi, -12)
	subl	$36, %esp
	movl	RAW_CLOSURE_CIF_OFFSET(%eax), %esi	 /* closure->cif */
	movl	RAW_CLOSURE_USER_DATA_OFFSET(%eax), %edx /* closure->user_data */
	movl	%edx, 12(%esp)	/* user_data */
	leal	8(%ebp), %edx	/* __builtin_dwarf_cfa () */
	movl	%edx, 8(%esp)	/* raw_args */
	leal	-24(%ebp), %edx
	movl	%edx, 4(%esp)	/* &res */
	movl	%esi, (%esp)	/* cif */
	call	*RAW_CLOSURE_FUN_OFFSET(%eax)		 /* closure->fun */
	movl	CIF_FLAGS_OFFSET(%esi), %eax		 /* rtype */
	cmpl	$FFI_TYPE_INT, %eax
	je	.Lrcls_retint

	/* Handle FFI_TYPE_UINT8, FFI_TYPE_SINT8, FFI_TYPE_UINT16,
	   FFI_TYPE_SINT16, FFI_TYPE_UINT32, FFI_TYPE_SINT32.  */
	cmpl	$FFI_TYPE_UINT64, %eax
	jge	0f
	cmpl	$FFI_TYPE_UINT8, %eax
	jge	.Lrcls_retint
0:
	cmpl	$FFI_TYPE_FLOAT, %eax
	je	.Lrcls_retfloat
	cmpl	$FFI_TYPE_DOUBLE, %eax
	je	.Lrcls_retdouble
	cmpl	$FFI_TYPE_LONGDOUBLE, %eax
	je	.Lrcls_retldouble
	cmpl	$FFI_TYPE_SINT64, %eax
	je	.Lrcls_retllong
.Lrcls_epilogue:
	addl	$36, %esp
	popl	%esi
	popl	%ebp
	ret
.Lrcls_retint:
	movl	-24(%ebp), %eax
	jmp	.Lrcls_epilogue
.Lrcls_retfloat:
	flds	-24(%ebp)
	jmp	.Lrcls_epilogue
.Lrcls_retdouble:
	fldl	-24(%ebp)
	jmp	.Lrcls_epilogue
.Lrcls_retldouble:
	fldt	-24(%ebp)
	jmp	.Lrcls_epilogue
.Lrcls_retllong:
	movl	-24(%ebp), %eax
	movl	-20(%ebp), %edx
	jmp	.Lrcls_epilogue
	cfi_endproc
	.size	ffi_closure_raw_SYSV, .-ffi_closure_raw_SYSV

#endif /* !FFI_NO_RAW_API */
#endif /* ifndef __x86_64__ */
#if defined __ELF__ && defined __linux__
	.section	.note.GNU-stack,"",@progbits
#endif

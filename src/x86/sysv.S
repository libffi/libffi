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

/* Handle win32 fastcall name mangling.  */
#ifdef X86_WIN32
# define ffi_call_i386		@ffi_call_i386@8
# define ffi_closure_inner	@ffi_closure_inner@8
#else
# define ffi_call_i386		C(ffi_call_i386)
# define ffi_closure_inner	C(ffi_closure_inner)
#endif

/* This macro allows the safe creation of jump tables without an
   actual table.  The entry points into the table are all 8 bytes.
   The use of ORG asserts that we're at the correct location.  */
/* ??? The clang assembler doesn't handle .org with symbolic expressions.  */
#if defined(__clang__) || defined(__APPLE__)
# define E(X)	.align 8
#else
# define E(X)	.align 8; .org 0b + X * 8
#endif

	.text
	.align	16
	.globl	ffi_call_i386
	FFI_HIDDEN(ffi_call_i386)

/* This is declared as

   void ffi_call_i386(struct call_frame *frame, char *argp)
        __attribute__((fastcall));

   Thus the arguments are present in

        ecx: frame
        edx: argp
*/

ffi_call_i386:
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
	cfi_rel_offset(%ebx, 8)

	andl	$X86_RET_TYPE_MASK, %ecx
#ifdef __PIC__
	call	C(__x86.get_pc_thunk.bx)
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
ENDF(ffi_call_i386)

/* The inner helper is declared as

   void ffi_closure_inner(struct closure_frame *frame, char *argp)
	__attribute_((fastcall))

   Thus the arguments are placed in

	ecx:	frame
	edx:	argp
*/

/* Macros to help setting up the closure_data structure.  */

#define closure_FS	(16 + 3*4 + 3*4 + 4)

#define FFI_CLOSURE_SAVE_REGS		\
	movl	%eax, 16+R_EAX*4(%esp);	\
	movl	%edx, 16+R_EDX*4(%esp);	\
	movl	%ecx, 16+R_ECX*4(%esp)

#define FFI_CLOSURE_COPY_TRAMP_DATA					\
	movl	FFI_TRAMPOLINE_SIZE(%eax), %edx;	/* copy cif */	\
	movl	FFI_TRAMPOLINE_SIZE+4(%eax), %ecx;	/* copy fun */	\
	movl	FFI_TRAMPOLINE_SIZE+8(%eax), %eax;	/* copy user_data */ \
	movl	%edx, 28(%esp);						\
	movl	%ecx, 32(%esp);						\
	movl	%eax, 36(%esp)

# define FFI_CLOSURE_CALL_INNER						\
	movl	%esp, %ecx;			/* load closure_data */	\
	leal	closure_FS+4(%esp), %edx;	/* load incoming stack */ \
	call	ffi_closure_inner
#define FFI_CLOSURE_MASK_AND_JUMP					\
	andl	$X86_RET_TYPE_MASK, %eax;				\
	leal	0f(, %eax, 8), %eax;					\
	jmp	*%eax

#ifdef __PIC__
# if defined X86_DARWIN || defined HAVE_HIDDEN_VISIBILITY_ATTRIBUTE
#  undef FFI_CLOSURE_MASK_AND_JUMP
#  define FFI_CLOSURE_MASK_AND_JUMP					\
	andl	$X86_RET_TYPE_MASK, %eax;				\
	call	C(__x86.get_pc_thunk.dx);				\
1:	leal	0f-1b(%edx, %eax, 8), %eax;				\
	jmp	*%eax
# else
#  undef FFI_CLOSURE_CALL_INNER
#  define FFI_CLOSURE_CALL_INNER					\
	movl	%esp, %ecx;			/* load closure_data */	\
	leal	closure_FS+4(%esp), %edx;	/* load incoming stack */ \
	movl	%ebx, 40(%esp);			/* save ebx */		\
	cfi_rel_offset(%ebx, 40);					\
	call	C(__x86.get_pc_thunk.bx);	/* load got register */	\
1:	addl	$C(_GLOBAL_OFFSET_TABLE_), %ebx;			\
	call	ffi_closure_inner@PLT
#  undef FFI_CLOSURE_MASK_AND_JUMP
#  define FFI_CLOSURE_MASK_AND_JUMP					\
	andl	$X86_RET_TYPE_MASK, %eax;				\
	leal	0f@GOTOFF(%ebx, %eax, 8), %eax;				\
	movl	40(%esp), %ebx;			/* restore ebx */	\
	cfi_restore(%ebx);						\
	jmp	*%eax
# endif /* DARWIN || HIDDEN */
#endif /* __PIC__ */

#define FFI_GO_CLOSURE(suffix, chain, t1, t2)				\
	.align	16;							\
	.globl	C(C1(ffi_go_closure_,suffix));				\
	FFI_HIDDEN(C(C1(ffi_go_closure_,suffix)));			\
C(C1(ffi_go_closure_,suffix)):						\
	cfi_startproc;							\
	subl	$closure_FS, %esp;					\
	/* Note clang bug 21515: adjust_cfa_offset error across endproc.  */ \
	cfi_def_cfa_offset(closure_FS + 4);				\
	FFI_CLOSURE_SAVE_REGS;						\
	movl	4(chain), t1;		/* copy cif */			\
	movl	8(chain), t2;		/* copy fun */			\
	movl	t1, 28(%esp);						\
	movl	t2, 32(%esp);						\
	movl	chain, 36(%esp);	/* closure is user_data */	\
	jmp	88f;							\
	cfi_endproc;							\
ENDF(C(C1(ffi_go_closure_,suffix)))

FFI_GO_CLOSURE(EAX, %eax, %edx, %ecx)
FFI_GO_CLOSURE(ECX, %ecx, %edx, %eax)

/* The closure entry points are reached from the ffi_closure trampoline.
   On entry, %eax contains the address of the ffi_closure.  */

	.align	16
	.globl	C(ffi_closure_i386)
	FFI_HIDDEN(C(ffi_closure_i386))

C(ffi_closure_i386):
	cfi_startproc
	subl	$closure_FS, %esp
	/* Note clang bug 21515: adjust_cfa_offset error across endproc.  */
	cfi_def_cfa_offset(closure_FS + 4)

	FFI_CLOSURE_SAVE_REGS
	FFI_CLOSURE_COPY_TRAMP_DATA

88:	/* Entry point from preceeding Go closures.  */

	FFI_CLOSURE_CALL_INNER
	FFI_CLOSURE_MASK_AND_JUMP

	.align	8
0:
E(X86_RET_FLOAT)
	flds	(%esp)
	jmp	9f
E(X86_RET_DOUBLE)
	fldl	(%esp)
	jmp	9f
E(X86_RET_LDOUBLE)
	fldt	(%esp)
	jmp	9f
E(X86_RET_SINT8)
	movsbl	(%esp), %eax
	jmp	9f
E(X86_RET_SINT16)
	movswl	(%esp), %eax
	jmp	9f
E(X86_RET_UINT8)
	movzbl	(%esp), %eax
	jmp	9f
E(X86_RET_UINT16)
	movzwl	(%esp), %eax
	jmp	9f
E(X86_RET_INT64)
	movl	4(%esp), %edx
	/* fallthru */
E(X86_RET_INT32)
	movl	(%esp), %eax
	/* fallthru */
E(X86_RET_VOID)
9:	addl	$closure_FS, %esp
	cfi_adjust_cfa_offset(-closure_FS)
	ret
	cfi_adjust_cfa_offset(closure_FS)
E(X86_RET_STRUCTPOP)
	addl	$closure_FS, %esp
	cfi_adjust_cfa_offset(-closure_FS)
	ret	$4
	cfi_adjust_cfa_offset(closure_FS)
E(X86_RET_STRUCTARG)
	movl	(%esp), %eax
	jmp	9b
E(X86_RET_STRUCT_1B)
	movzbl	(%esp), %eax
	jmp	9b
E(X86_RET_STRUCT_2B)
	movzwl	(%esp), %eax
	jmp	9b

	/* Fill out the table so that bad values are predictable.  */
E(X86_RET_UNUSED14)
	ud2
E(X86_RET_UNUSED15)
	ud2

	cfi_endproc
ENDF(C(ffi_closure_i386))

FFI_GO_CLOSURE(STDCALL, %ecx, %edx, %eax)

/* For REGISTER, we have no available parameter registers, and so we
   enter here having pushed the closure onto the stack.  */

	.align	16
	.globl	C(ffi_closure_REGISTER)
	FFI_HIDDEN(C(ffi_closure_REGISTER))
C(ffi_closure_REGISTER):
	cfi_startproc
	cfi_def_cfa(%esp, 8)
	cfi_offset(%eip, -8)
	subl	$closure_FS-4, %esp
	/* Note clang bug 21515: adjust_cfa_offset error across endproc.  */
	cfi_def_cfa_offset(closure_FS + 4)

	FFI_CLOSURE_SAVE_REGS

	movl	closure_FS-4(%esp), %ecx	/* load retaddr */
	movl	closure_FS(%esp), %eax		/* load closure */
	movl	%ecx, closure_FS(%esp)		/* move retaddr */
	jmp	0f

	cfi_endproc
ENDF(C(ffi_closure_REGISTER))

/* For STDCALL (and others), we need to pop N bytes of arguments off
   the stack following the closure.  The amount needing to be popped
   is returned to us from ffi_closure_inner.  */

	.align	16
	.globl	C(ffi_closure_STDCALL)
	FFI_HIDDEN(C(ffi_closure_STDCALL))
C(ffi_closure_STDCALL):
	cfi_startproc
	subl	$closure_FS, %esp
	/* Note clang bug 21515: adjust_cfa_offset error across endproc.  */
	cfi_def_cfa_offset(closure_FS + 4)

	FFI_CLOSURE_SAVE_REGS

0:	/* Entry point from ffi_closure_REGISTER.  */

	FFI_CLOSURE_COPY_TRAMP_DATA

88:	/* Entry point from preceeding Go closure.  */

	FFI_CLOSURE_CALL_INNER

	movl	%eax, %ecx
	shrl	$X86_RET_POP_SHIFT, %ecx	/* isolate pop count */
	leal	closure_FS(%esp, %ecx), %ecx	/* compute popped esp */
	movl	closure_FS(%esp), %edx		/* move return address */
	movl	%edx, (%ecx)

	/* New pseudo-stack frame based off ecx.  This is unwind trickery
	   in that the CFA *has* changed, to the proper popped stack address.
	   Note that the location to which we moved the return address
	   is the new CFA-4, so that's unchanged.  */
	cfi_def_cfa(%ecx, 4)
	/* Normally esp is unwound to CFA + the caller's ARGS_SIZE.
	   We've just set the CFA to that final value.  Tell the unwinder
	   to restore esp from CFA without the ARGS_SIZE:
	   DW_CFA_val_expression %esp, DW_OP_call_frame_cfa.  */
	cfi_escape(0x16, 4, 1, 0x9c)

	FFI_CLOSURE_MASK_AND_JUMP

	.align	8
0:
E(X86_RET_FLOAT)
	flds    (%esp)
	movl    %ecx, %esp
	ret
E(X86_RET_DOUBLE)
	fldl    (%esp)
	movl    %ecx, %esp
	ret
E(X86_RET_LDOUBLE)
	fldt    (%esp)
	movl    %ecx, %esp
	ret
E(X86_RET_SINT8)
	movsbl  (%esp), %eax
	movl    %ecx, %esp
	ret
E(X86_RET_SINT16)
	movswl  (%esp), %eax
	movl    %ecx, %esp
	ret
E(X86_RET_UINT8)
	movzbl  (%esp), %eax
	movl    %ecx, %esp
	ret
E(X86_RET_UINT16)
	movzwl  (%esp), %eax
	movl    %ecx, %esp
	ret
E(X86_RET_INT64)
	popl    %eax
	popl    %edx
	movl    %ecx, %esp
	ret
E(X86_RET_INT32)
	movl    (%esp), %eax
	movl    %ecx, %esp
	ret
E(X86_RET_VOID)
	movl    %ecx, %esp
	ret
E(X86_RET_STRUCTPOP)
	movl    %ecx, %esp
	ret
E(X86_RET_STRUCTARG)
	movl	(%esp), %eax
	movl	%ecx, %esp
	ret
E(X86_RET_STRUCT_1B)
	movzbl	(%esp), %eax
	movl	%ecx, %esp
	ret
E(X86_RET_STRUCT_2B)
	movzwl	(%esp), %eax
	movl	%ecx, %esp
	ret

	/* Fill out the table so that bad values are predictable.  */
E(X86_RET_UNUSED14)
	ud2
E(X86_RET_UNUSED15)
	ud2

	cfi_endproc
ENDF(C(ffi_closure_STDCALL))

#if !FFI_NO_RAW_API

#define raw_closure_S_FS	(16+16+12)

	.align	16
	.globl	C(ffi_closure_raw_SYSV)
	FFI_HIDDEN(C(ffi_closure_raw_SYSV))
C(ffi_closure_raw_SYSV):
	cfi_startproc
	subl	$raw_closure_S_FS, %esp
	/* Note clang bug 21515: adjust_cfa_offset error across endproc.  */
	cfi_def_cfa_offset(raw_closure_S_FS + 4)
	movl	%ebx, raw_closure_S_FS-4(%esp)
	cfi_rel_offset(%ebx, raw_closure_S_FS-4)

	movl	FFI_TRAMPOLINE_SIZE+8(%eax), %edx	/* load cl->user_data */
	movl	%edx, 12(%esp)
	leal	raw_closure_S_FS+4(%esp), %edx		/* load raw_args */
	movl	%edx, 8(%esp)
	leal	16(%esp), %edx				/* load &res */
	movl	%edx, 4(%esp)
	movl	FFI_TRAMPOLINE_SIZE(%eax), %ebx		/* load cl->cif */
	movl	%ebx, (%esp)
	call	*FFI_TRAMPOLINE_SIZE+4(%eax)		/* call cl->fun */

	movl	20(%ebx), %eax				/* load cif->flags */
	andl	$X86_RET_TYPE_MASK, %eax
#ifdef __PIC__
	call	C(__x86.get_pc_thunk.bx)
1:	leal	0f-1b(%ebx, %eax, 8), %eax
#else
	leal	0f(,%eax, 8), %eax
#endif
	movl	raw_closure_S_FS-4(%esp), %ebx
	cfi_restore(%ebx)
	jmp	*%eax

	.align	8
0:
E(X86_RET_FLOAT)
	flds	16(%esp)
	jmp	9f
E(X86_RET_DOUBLE)
	fldl	16(%esp)
	jmp	9f
E(X86_RET_LDOUBLE)
	fldt	16(%esp)
	jmp	9f
E(X86_RET_SINT8)
	movsbl	16(%esp), %eax
	jmp	9f
E(X86_RET_SINT16)
	movswl	16(%esp), %eax
	jmp	9f
E(X86_RET_UINT8)
	movzbl	16(%esp), %eax
	jmp	9f
E(X86_RET_UINT16)
	movzwl	16(%esp), %eax
	jmp	9f
E(X86_RET_INT64)
	movl	16+4(%esp), %edx
	/* fallthru */
E(X86_RET_INT32)
	movl	16(%esp), %eax
	/* fallthru */
E(X86_RET_VOID)
9:	addl	$raw_closure_S_FS, %esp
	cfi_adjust_cfa_offset(-raw_closure_S_FS)
	ret
	cfi_adjust_cfa_offset(raw_closure_S_FS)
E(X86_RET_STRUCTPOP)
	addl	$raw_closure_S_FS, %esp
	cfi_adjust_cfa_offset(-raw_closure_S_FS)
	ret	$4
	cfi_adjust_cfa_offset(raw_closure_S_FS)
E(X86_RET_STRUCTARG)
	movl	16(%esp), %eax
	jmp	9b
E(X86_RET_STRUCT_1B)
	movzbl	16(%esp), %eax
	jmp	9b
E(X86_RET_STRUCT_2B)
	movzwl	16(%esp), %eax
	jmp	9b

	/* Fill out the table so that bad values are predictable.  */
E(X86_RET_UNUSED14)
	ud2
E(X86_RET_UNUSED15)
	ud2

	cfi_endproc
ENDF(C(ffi_closure_raw_SYSV))

#undef	raw_closure_S_FS
#define raw_closure_T_FS	(16+16+8)

	.align	16
	.globl	C(ffi_closure_raw_THISCALL)
	FFI_HIDDEN(C(ffi_closure_raw_THISCALL))
C(ffi_closure_raw_THISCALL):
	cfi_startproc
	/* Rearrange the stack such that %ecx is the first argument.
	   This means moving the return address.  */
	popl	%edx
	/* Note clang bug 21515: adjust_cfa_offset error across endproc.  */
	cfi_def_cfa_offset(0)
	cfi_register(%eip, %edx)
	pushl	%ecx
	cfi_adjust_cfa_offset(4)
	pushl	%edx
	cfi_adjust_cfa_offset(4)
	cfi_rel_offset(%eip, 0)
	subl	$raw_closure_T_FS, %esp
	cfi_adjust_cfa_offset(raw_closure_T_FS)
	movl	%ebx, raw_closure_T_FS-4(%esp)
	cfi_rel_offset(%ebx, raw_closure_T_FS-4)

	movl	FFI_TRAMPOLINE_SIZE+8(%eax), %edx	/* load cl->user_data */
	movl	%edx, 12(%esp)
	leal	raw_closure_T_FS+4(%esp), %edx		/* load raw_args */
	movl	%edx, 8(%esp)
	leal	16(%esp), %edx				/* load &res */
	movl	%edx, 4(%esp)
	movl	FFI_TRAMPOLINE_SIZE(%eax), %ebx		/* load cl->cif */
	movl	%ebx, (%esp)
	call	*FFI_TRAMPOLINE_SIZE+4(%eax)		/* call cl->fun */

	movl	20(%ebx), %eax				/* load cif->flags */
	andl	$X86_RET_TYPE_MASK, %eax
#ifdef __PIC__
	call	C(__x86.get_pc_thunk.bx)
1:	leal	0f-1b(%ebx, %eax, 8), %eax
#else
	leal	0f(,%eax, 8), %eax
#endif
	movl	raw_closure_T_FS-4(%esp), %ebx
	cfi_restore(%ebx)
	jmp	*%eax

	.align	8
0:
E(X86_RET_FLOAT)
	flds	16(%esp)
	jmp	9f
E(X86_RET_DOUBLE)
	fldl	16(%esp)
	jmp	9f
E(X86_RET_LDOUBLE)
	fldt	16(%esp)
	jmp	9f
E(X86_RET_SINT8)
	movsbl	16(%esp), %eax
	jmp	9f
E(X86_RET_SINT16)
	movswl	16(%esp), %eax
	jmp	9f
E(X86_RET_UINT8)
	movzbl	16(%esp), %eax
	jmp	9f
E(X86_RET_UINT16)
	movzwl	16(%esp), %eax
	jmp	9f
E(X86_RET_INT64)
	movl	16+4(%esp), %edx
	/* fallthru */
E(X86_RET_INT32)
	movl	16(%esp), %eax
	/* fallthru */
E(X86_RET_VOID)
9:	addl	$raw_closure_T_FS, %esp
	cfi_adjust_cfa_offset(-raw_closure_T_FS)
	/* Remove the extra %ecx argument we pushed.  */
	ret	$4
	cfi_adjust_cfa_offset(raw_closure_T_FS)
E(X86_RET_STRUCTPOP)
	addl	$raw_closure_T_FS, %esp
	cfi_adjust_cfa_offset(-raw_closure_T_FS)
	ret	$8
	cfi_adjust_cfa_offset(raw_closure_T_FS)
E(X86_RET_STRUCTARG)
	movl	16(%esp), %eax
	jmp	9b
E(X86_RET_STRUCT_1B)
	movzbl	16(%esp), %eax
	jmp	9b
E(X86_RET_STRUCT_2B)
	movzwl	16(%esp), %eax
	jmp	9b

	/* Fill out the table so that bad values are predictable.  */
E(X86_RET_UNUSED14)
	ud2
E(X86_RET_UNUSED15)
	ud2

	cfi_endproc
ENDF(C(ffi_closure_raw_THISCALL))

#endif /* !FFI_NO_RAW_API */

#ifdef X86_DARWIN
# define COMDAT(X)							\
        .section __TEXT,__textcoal_nt,coalesced,pure_instructions;	\
        .weak_definition X;						\
        .private_extern X
#elif defined __ELF__
# define COMDAT(X)							\
	.section .text.X,"axG",@progbits,X,comdat;			\
	.globl	X;							\
	FFI_HIDDEN(X)
#else
# define COMDAT(X)
#endif

#if defined(__PIC__)
	COMDAT(C(__x86.get_pc_thunk.bx))
C(__x86.get_pc_thunk.bx):
	cfi_startproc
	movl	(%esp), %ebx
	ret
	cfi_endproc
ENDF(C(__x86.get_pc_thunk.bx))
# if defined X86_DARWIN || defined HAVE_HIDDEN_VISIBILITY_ATTRIBUTE
	COMDAT(C(__x86.get_pc_thunk.dx))
C(__x86.get_pc_thunk.dx):
	cfi_startproc
	movl	(%esp), %edx
	ret
	cfi_endproc
ENDF(C(__x86.get_pc_thunk.dx))
#endif /* DARWIN || HIDDEN */
#endif /* __PIC__ */

#endif /* ifndef __x86_64__ */

#if defined __ELF__ && defined __linux__
	.section	.note.GNU-stack,"",@progbits
#endif

/* -----------------------------------------------------------------------
   unix64.S - Copyright (c) 2013  The Written Word, Inc.
	    - Copyright (c) 2008  Red Hat, Inc
	    - Copyright (c) 2002  Bo Thorsen <bo@suse.de>

   x86-64 Foreign Function Interface 

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

#ifdef __x86_64__
#define LIBFFI_ASM	
#include <fficonfig.h>
#include <ffi.h>
#include <ffi_cfi.h>
#include "internal64.h"

	.text

/* This macro allows the safe creation of jump tables without an
   actual table.  The entry points into the table are all 8 bytes.
   The use of ORG asserts that we're at the correct location.  */
/* ??? The clang assembler doesn't handle .org with symbolic expressions.  */
.macro E index
	.align	8
#ifndef __clang__
	.org	0b + \index * 8, 0x90
#endif
.endm

/* ffi_call_unix64 (void *args, unsigned long bytes, unsigned flags,
	            void *raddr, void (*fnaddr)(void));

   Bit o trickiness here -- ARGS+BYTES is the base of the stack frame
   for this function.  This has been allocated by ffi_call.  We also
   deallocate some of the stack that has been alloca'd.  */

	.align	8
	.globl	ffi_call_unix64
	.type	ffi_call_unix64,@function
	FFI_HIDDEN(ffi_call_unix64)

ffi_call_unix64:
	cfi_startproc
	movq	(%rsp), %r10		/* Load return address.  */
	leaq	(%rdi, %rsi), %rax	/* Find local stack base.  */
	movq	%rdx, (%rax)		/* Save flags.  */
	movq	%rcx, 8(%rax)		/* Save raddr.  */
	movq	%rbp, 16(%rax)		/* Save old frame pointer.  */
	movq	%r10, 24(%rax)		/* Relocate return address.  */
	movq	%rax, %rbp		/* Finalize local stack frame.  */

	/* New stack frame based off rbp.  This is a itty bit of unwind
	   trickery in that the CFA *has* changed.  There is no easy way
	   to describe it correctly on entry to the function.  Fortunately,
	   it doesn't matter too much since at all points we can correctly
	   unwind back to ffi_call.  Note that the location to which we
	   moved the return address is (the new) CFA-8, so from the
	   perspective of the unwind info, it hasn't moved.  */
	cfi_def_cfa(%rbp, 32)
	cfi_rel_offset(%rbp, 16)

	movq	%rdi, %r10		/* Save a copy of the register area. */
	movq	%r8, %r11		/* Save a copy of the target fn.  */
	movl	%r9d, %eax		/* Set number of SSE registers.  */

	/* Load up all argument registers.  */
	movq	(%r10), %rdi
	movq	0x08(%r10), %rsi
	movq	0x10(%r10), %rdx
	movq	0x18(%r10), %rcx
	movq	0x20(%r10), %r8
	movq	0x28(%r10), %r9
	movl	0xb0(%r10), %eax
	testl	%eax, %eax
	jnz	.Lload_sse
.Lret_from_load_sse:

	/* Deallocate the reg arg area, except for r10, then load via pop.  */
	leaq	0xb8(%r10), %rsp
	popq	%r10

	/* Call the user function.  */
	call	*%r11

	/* Deallocate stack arg area; local stack frame in redzone.  */
	leaq	24(%rbp), %rsp

	movq	0(%rbp), %rcx		/* Reload flags.  */
	movq	8(%rbp), %rdi		/* Reload raddr.  */
	movq	16(%rbp), %rbp		/* Reload old frame pointer.  */
	cfi_remember_state
	cfi_def_cfa(%rsp, 8)
	cfi_restore(%rbp)

	/* The first byte of the flags contains the FFI_TYPE.  */
	cmpb	$UNIX64_RET_LAST, %cl
	movzbl	%cl, %r10d
	leaq	0f(%rip), %r11
	ja	9f
	leaq	(%r11, %r10, 8), %r10

	/* Prep for the structure cases: scratch area in redzone.  */
	leaq	-20(%rsp), %rsi
	jmp	*%r10

	.align	8
0:
E UNIX64_RET_VOID
	ret
E UNIX64_RET_UINT8
	movzbl	%al, %eax
	movq	%rax, (%rdi)
	ret
E UNIX64_RET_UINT16
	movzwl	%ax, %eax
	movq	%rax, (%rdi)
	ret
E UNIX64_RET_UINT32
	movl	%eax, %eax
	movq	%rax, (%rdi)
	ret
E UNIX64_RET_SINT8
	movsbq	%al, %rax
	movq	%rax, (%rdi)
	ret
E UNIX64_RET_SINT16
	movswq	%ax, %rax
	movq	%rax, (%rdi)
	ret
E UNIX64_RET_SINT32
	cltq
	movq	%rax, (%rdi)
	ret
E UNIX64_RET_INT64
	movq	%rax, (%rdi)
	ret
E UNIX64_RET_XMM32
	movd	%xmm0, (%rdi)
	ret
E UNIX64_RET_XMM64
	movq	%xmm0, (%rdi)
	ret
E UNIX64_RET_X87
	fstpt	(%rdi)
	ret
E UNIX64_RET_X87_2
	fstpt	(%rdi)
	fstpt	16(%rdi)
	ret
E UNIX64_RET_ST_XMM0_RAX
	movq	%rax, 8(%rsi)
	jmp	3f
E UNIX64_RET_ST_RAX_XMM0
	movq	%xmm0, 8(%rsi)
	jmp	2f
E UNIX64_RET_ST_XMM0_XMM1
	movq	%xmm1, 8(%rsi)
	jmp	3f
E UNIX64_RET_ST_RAX_RDX
	movq	%rdx, 8(%rsi)
2:	movq	%rax, (%rsi)
	shrl	$UNIX64_SIZE_SHIFT, %ecx
	rep movsb
	ret
	.align 8
3:	movq	%xmm0, (%rsi)
	shrl	$UNIX64_SIZE_SHIFT, %ecx
	rep movsb
	ret

9:	call	abort@PLT

	/* Many times we can avoid loading any SSE registers at all.
	   It's not worth an indirect jump to load the exact set of
	   SSE registers needed; zero or all is a good compromise.  */
	.align 2
	cfi_restore_state
.Lload_sse:
	movdqa	0x30(%r10), %xmm0
	movdqa	0x40(%r10), %xmm1
	movdqa	0x50(%r10), %xmm2
	movdqa	0x60(%r10), %xmm3
	movdqa	0x70(%r10), %xmm4
	movdqa	0x80(%r10), %xmm5
	movdqa	0x90(%r10), %xmm6
	movdqa	0xa0(%r10), %xmm7
	jmp	.Lret_from_load_sse

	cfi_endproc
	.size    ffi_call_unix64,.-ffi_call_unix64

/* 6 general registers, 8 vector registers,
   32 bytes of rvalue, 8 bytes of alignment.  */
#define ffi_closure_OFS_G	0
#define ffi_closure_OFS_V	(6*8)
#define ffi_closure_OFS_RVALUE	(ffi_closure_OFS_V + 8*16)
#define ffi_closure_FS		(ffi_closure_OFS_RVALUE + 32 + 8)

/* The location of rvalue within the red zone after deallocating the frame.  */
#define ffi_closure_RED_RVALUE	(ffi_closure_OFS_RVALUE - ffi_closure_FS)

	.align	2
	.globl	ffi_closure_unix64_sse
	.type	ffi_closure_unix64_sse,@function
	FFI_HIDDEN(ffi_closure_unix64_sse)

ffi_closure_unix64_sse:
	cfi_startproc
	subq	$ffi_closure_FS, %rsp
	/* Note clang bug 21515: adjust_cfa_offset error across endproc.  */
	cfi_def_cfa_offset(ffi_closure_FS + 8)

	movdqa	%xmm0, ffi_closure_OFS_V+0x00(%rsp)
	movdqa	%xmm1, ffi_closure_OFS_V+0x10(%rsp)
	movdqa	%xmm2, ffi_closure_OFS_V+0x20(%rsp)
	movdqa	%xmm3, ffi_closure_OFS_V+0x30(%rsp)
	movdqa	%xmm4, ffi_closure_OFS_V+0x40(%rsp)
	movdqa	%xmm5, ffi_closure_OFS_V+0x50(%rsp)
	movdqa	%xmm6, ffi_closure_OFS_V+0x60(%rsp)
	movdqa	%xmm7, ffi_closure_OFS_V+0x70(%rsp)
	jmp	0f

	cfi_endproc
	.size	ffi_closure_unix64_sse,.-ffi_closure_unix64_sse

	.align	2
	.globl	ffi_closure_unix64
	.type	ffi_closure_unix64,@function
	FFI_HIDDEN(ffi_closure_unix64)

ffi_closure_unix64:
	cfi_startproc
	subq	$ffi_closure_FS, %rsp
	/* Note clang bug 21515: adjust_cfa_offset error across endproc.  */
	cfi_def_cfa_offset(ffi_closure_FS + 8)
0:
	movq	%rdi, ffi_closure_OFS_G+0x00(%rsp)
	movq    %rsi, ffi_closure_OFS_G+0x08(%rsp)
	movq    %rdx, ffi_closure_OFS_G+0x10(%rsp)
	movq    %rcx, ffi_closure_OFS_G+0x18(%rsp)
	movq    %r8,  ffi_closure_OFS_G+0x20(%rsp)
	movq    %r9,  ffi_closure_OFS_G+0x28(%rsp)

#ifdef __ILP32__
	movl	FFI_TRAMPOLINE_SIZE(%r10), %edi		/* Load cif */
	movl	FFI_TRAMPOLINE_SIZE+4(%r10), %esi	/* Load fun */
	movl	FFI_TRAMPOLINE_SIZE+8(%r10), %edx	/* Load user_data */
#else
	movq	FFI_TRAMPOLINE_SIZE(%r10), %rdi		/* Load cif */
	movq	FFI_TRAMPOLINE_SIZE+8(%r10), %rsi	/* Load fun */
	movq	FFI_TRAMPOLINE_SIZE+16(%r10), %rdx	/* Load user_data */
#endif
.Ldo_closure:
	leaq	ffi_closure_OFS_RVALUE(%rsp), %rcx	/* Load rvalue */
	movq	%rsp, %r8				/* Load reg_args */
	leaq	ffi_closure_FS+8(%rsp), %r9		/* Load argp */
	call	ffi_closure_unix64_inner

	/* Deallocate stack frame early; return value is now in redzone.  */
	addq	$ffi_closure_FS, %rsp
	cfi_adjust_cfa_offset(-ffi_closure_FS)

	/* The first byte of the return value contains the FFI_TYPE.  */
	cmpb	$UNIX64_RET_LAST, %al
	movzbl	%al, %r10d
	leaq	0f(%rip), %r11
	ja	9f
	leaq	(%r11, %r10, 8), %r10
	leaq	ffi_closure_RED_RVALUE(%rsp), %rsi
	jmp	*%r10

	.align	8
0:
E UNIX64_RET_VOID
	ret
E UNIX64_RET_UINT8
	movzbl	(%rsi), %eax
	ret
E UNIX64_RET_UINT16
	movzwl	(%rsi), %eax
	ret
E UNIX64_RET_UINT32
	movl	(%rsi), %eax
	ret
E UNIX64_RET_SINT8
	movsbl	(%rsi), %eax
	ret
E UNIX64_RET_SINT16
	movswl	(%rsi), %eax
	ret
E UNIX64_RET_SINT32
	movl	(%rsi), %eax
	ret
E UNIX64_RET_INT64
	movq	(%rsi), %rax
	ret
E UNIX64_RET_XMM32
	movd	(%rsi), %xmm0
	ret
E UNIX64_RET_XMM64
	movq	(%rsi), %xmm0
	ret
E UNIX64_RET_X87
	fldt	(%rsi)
	ret
E UNIX64_RET_X87_2
	fldt	16(%rsi)
	fldt	(%rsi)
	ret
E UNIX64_RET_ST_XMM0_RAX
	movq	8(%rsi), %rax
	jmp	3f
E UNIX64_RET_ST_RAX_XMM0
	movq	8(%rsi), %xmm0
	jmp	2f
E UNIX64_RET_ST_XMM0_XMM1
	movq	8(%rsi), %xmm1
	jmp	3f
E UNIX64_RET_ST_RAX_RDX
	movq	8(%rsi), %rdx
2:	movq	(%rsi), %rax
	ret
	.align	8
3:	movq	(%rsi), %xmm0
	ret

9:	call	abort@PLT

	cfi_endproc
	.size	ffi_closure_unix64,.-ffi_closure_unix64

	.align	2
	.globl	ffi_go_closure_unix64_sse
	.type	ffi_go_closure_unix64_sse,@function
	FFI_HIDDEN(ffi_go_closure_unix64_sse)

ffi_go_closure_unix64_sse:
	cfi_startproc
	subq	$ffi_closure_FS, %rsp
	/* Note clang bug 21515: adjust_cfa_offset error across endproc.  */
	cfi_def_cfa_offset(ffi_closure_FS + 8)

	movdqa	%xmm0, ffi_closure_OFS_V+0x00(%rsp)
	movdqa	%xmm1, ffi_closure_OFS_V+0x10(%rsp)
	movdqa	%xmm2, ffi_closure_OFS_V+0x20(%rsp)
	movdqa	%xmm3, ffi_closure_OFS_V+0x30(%rsp)
	movdqa	%xmm4, ffi_closure_OFS_V+0x40(%rsp)
	movdqa	%xmm5, ffi_closure_OFS_V+0x50(%rsp)
	movdqa	%xmm6, ffi_closure_OFS_V+0x60(%rsp)
	movdqa	%xmm7, ffi_closure_OFS_V+0x70(%rsp)
	jmp	0f

	cfi_endproc
	.size	ffi_go_closure_unix64_sse,.-ffi_go_closure_unix64_sse

	.align	2
	.globl	ffi_go_closure_unix64
	.type	ffi_go_closure_unix64,@function
	FFI_HIDDEN(ffi_go_closure_unix64)

ffi_go_closure_unix64:
	cfi_startproc
	subq	$ffi_closure_FS, %rsp
	/* Note clang bug 21515: adjust_cfa_offset error across endproc.  */
	cfi_def_cfa_offset(ffi_closure_FS + 8)
0:
	movq	%rdi, ffi_closure_OFS_G+0x00(%rsp)
	movq    %rsi, ffi_closure_OFS_G+0x08(%rsp)
	movq    %rdx, ffi_closure_OFS_G+0x10(%rsp)
	movq    %rcx, ffi_closure_OFS_G+0x18(%rsp)
	movq    %r8,  ffi_closure_OFS_G+0x20(%rsp)
	movq    %r9,  ffi_closure_OFS_G+0x28(%rsp)

#ifdef __ILP32__
	movl	4(%r10), %edi		/* Load cif */
	movl	8(%r10), %esi		/* Load fun */
	movl	%r10d, %edx		/* Load closure (user_data) */
#else
	movq	8(%r10), %rdi		/* Load cif */
	movq	16(%r10), %rsi		/* Load fun */
	movq	%r10, %rdx		/* Load closure (user_data) */
#endif
	jmp	.Ldo_closure

	cfi_endproc
	.size	ffi_go_closure_unix64,.-ffi_go_closure_unix64

#endif /* __x86_64__ */
#if defined __ELF__ && defined __linux__
	.section	.note.GNU-stack,"",@progbits
#endif

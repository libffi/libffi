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

.text

/* ffi_call_unix64 (void *args, unsigned long bytes, unsigned flags,
	            void *raddr, void (*fnaddr)(void));

   Bit o trickiness here -- ARGS+BYTES is the base of the stack frame
   for this function.  This has been allocated by ffi_call.  We also
   deallocate some of the stack that has been alloca'd.  */

	.align	2
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
	movzbl	%cl, %r10d
	leaq	.Lstore_table(%rip), %r11
	movslq	(%r11, %r10, 4), %r10
	addq	%r11, %r10
	jmp	*%r10

	.section .rodata
	.align	2
.Lstore_table:
	.long	.Lst_void-.Lstore_table		/* FFI_TYPE_VOID */
	.long	.Lst_sint32-.Lstore_table	/* FFI_TYPE_INT */
	.long	.Lst_float-.Lstore_table	/* FFI_TYPE_FLOAT */
	.long	.Lst_double-.Lstore_table	/* FFI_TYPE_DOUBLE */
	.long	.Lst_ldouble-.Lstore_table	/* FFI_TYPE_LONGDOUBLE */
	.long	.Lst_uint8-.Lstore_table	/* FFI_TYPE_UINT8 */
	.long	.Lst_sint8-.Lstore_table	/* FFI_TYPE_SINT8 */
	.long	.Lst_uint16-.Lstore_table	/* FFI_TYPE_UINT16 */
	.long	.Lst_sint16-.Lstore_table	/* FFI_TYPE_SINT16 */
	.long	.Lst_uint32-.Lstore_table	/* FFI_TYPE_UINT32 */
	.long	.Lst_sint32-.Lstore_table	/* FFI_TYPE_SINT32 */
	.long	.Lst_int64-.Lstore_table	/* FFI_TYPE_UINT64 */
	.long	.Lst_int64-.Lstore_table	/* FFI_TYPE_SINT64 */
	.long	.Lst_struct-.Lstore_table	/* FFI_TYPE_STRUCT */
	.long	.Lst_int64-.Lstore_table	/* FFI_TYPE_POINTER */
	.previous

	.align 2
.Lst_void:
	ret
	.align 2

.Lst_uint8:
	movzbq	%al, %rax
	movq	%rax, (%rdi)
	ret
	.align 2
.Lst_sint8:
	movsbq	%al, %rax
	movq	%rax, (%rdi)
	ret
	.align 2
.Lst_uint16:
	movzwq	%ax, %rax
	movq	%rax, (%rdi)
	.align 2
.Lst_sint16:
	movswq	%ax, %rax
	movq	%rax, (%rdi)
	ret
	.align 2
.Lst_uint32:
	movl	%eax, %eax
	movq	%rax, (%rdi)
	.align 2
.Lst_sint32:
	cltq
	movq	%rax, (%rdi)
	ret
	.align 2
.Lst_int64:
	movq	%rax, (%rdi)
	ret

	.align 2
.Lst_float:
	movss	%xmm0, (%rdi)
	ret
	.align 2
.Lst_double:
	movsd	%xmm0, (%rdi)
	ret
.Lst_ldouble:
	fstpt	(%rdi)
	ret

	.align 2
.Lst_struct:
	leaq	-20(%rsp), %rsi		/* Scratch area in redzone.  */

	/* We have to locate the values now, and since we don't want to
	   write too much data into the user's return value, we spill the
	   value to a 16 byte scratch area first.  Bits 8, 9, and 10
	   control where the values are located.  Only one of the three
	   bits will be set; see ffi_prep_cif_machdep for the pattern.  */
	movd	%xmm0, %r10
	movd	%xmm1, %r11
	testl	$0x100, %ecx
	cmovnz	%rax, %rdx
	cmovnz	%r10, %rax
	testl	$0x200, %ecx
	cmovnz	%r10, %rdx
	testl	$0x400, %ecx
	cmovnz	%r10, %rax
	cmovnz	%r11, %rdx
	movq	%rax, (%rsi)
	movq	%rdx, 8(%rsi)

	/* Bits 12-31 contain the true size of the structure.  Copy from
	   the scratch area to the true destination.  */
	shrl	$12, %ecx
	rep movsb
	ret

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
   16 bytes of rvalue, 8 bytes of alignment.  */
#define ffi_closure_OFS_G	0
#define ffi_closure_OFS_V	(6*8)
#define ffi_closure_OFS_RVALUE	(ffi_closure_OFS_V + 8*16)
#define ffi_closure_FS		(ffi_closure_OFS_RVALUE + 16 + 8)

/* The location of rvalue within the red zone after deallocating the frame.  */
#define ffi_closure_RED_RVALUE	(ffi_closure_OFS_RVALUE - ffi_closure_FS)

	.align	2
	.globl	ffi_closure_unix64_sse
	.type	ffi_closure_unix64_sse,@function
	FFI_HIDDEN(ffi_closure_unix64_sse)

ffi_closure_unix64_sse:
	cfi_startproc
	subq	$ffi_closure_FS, %rsp
	cfi_adjust_cfa_offset(ffi_closure_FS)

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
	cfi_adjust_cfa_offset(ffi_closure_FS)
0:
	movq	%rdi, ffi_closure_OFS_G+0x00(%rsp)
	movq    %rsi, ffi_closure_OFS_G+0x08(%rsp)
	movq    %rdx, ffi_closure_OFS_G+0x10(%rsp)
	movq    %rcx, ffi_closure_OFS_G+0x18(%rsp)
	movq    %r8,  ffi_closure_OFS_G+0x20(%rsp)
	movq    %r9,  ffi_closure_OFS_G+0x28(%rsp)

	movq	24(%r10), %rdi				/* Load cif */
	movq	32(%r10), %rsi				/* Load fun */
	movq	40(%r10), %rdx				/* Load user_data */
.Ldo_closure:
	leaq	ffi_closure_OFS_RVALUE(%rsp), %rcx	/* Load rvalue */
	movq	%rsp, %r8				/* Load reg_args */
	leaq	ffi_closure_FS+8(%rsp), %r9		/* Load argp */
	call	ffi_closure_unix64_inner

	/* Deallocate stack frame early; return value is now in redzone.  */
	addq	$ffi_closure_FS, %rsp
	cfi_adjust_cfa_offset(-ffi_closure_FS)

	/* The first byte of the return value contains the FFI_TYPE.  */
	movzbl	%al, %r10d
	leaq	.Lload_table(%rip), %r11
	movslq	(%r11, %r10, 4), %r10
	addq	%r11, %r10
	jmp	*%r10

	.section .rodata
	.align	2
.Lload_table:
	.long	.Lld_void-.Lload_table		/* FFI_TYPE_VOID */
	.long	.Lld_int32-.Lload_table		/* FFI_TYPE_INT */
	.long	.Lld_float-.Lload_table		/* FFI_TYPE_FLOAT */
	.long	.Lld_double-.Lload_table	/* FFI_TYPE_DOUBLE */
	.long	.Lld_ldouble-.Lload_table	/* FFI_TYPE_LONGDOUBLE */
	.long	.Lld_int8-.Lload_table		/* FFI_TYPE_UINT8 */
	.long	.Lld_int8-.Lload_table		/* FFI_TYPE_SINT8 */
	.long	.Lld_int16-.Lload_table		/* FFI_TYPE_UINT16 */
	.long	.Lld_int16-.Lload_table		/* FFI_TYPE_SINT16 */
	.long	.Lld_int32-.Lload_table		/* FFI_TYPE_UINT32 */
	.long	.Lld_int32-.Lload_table		/* FFI_TYPE_SINT32 */
	.long	.Lld_int64-.Lload_table		/* FFI_TYPE_UINT64 */
	.long	.Lld_int64-.Lload_table		/* FFI_TYPE_SINT64 */
	.long	.Lld_struct-.Lload_table	/* FFI_TYPE_STRUCT */
	.long	.Lld_int64-.Lload_table		/* FFI_TYPE_POINTER */
	.previous

	.align 2
.Lld_void:
	ret

	.align 2
.Lld_int8:
	movzbl	ffi_closure_RED_RVALUE(%rsp), %eax
	ret
	.align 2
.Lld_int16:
	movzwl	ffi_closure_RED_RVALUE(%rsp), %eax
	ret
	.align 2
.Lld_int32:
	movl	ffi_closure_RED_RVALUE(%rsp), %eax
	ret
	.align 2
.Lld_int64:
	movq	ffi_closure_RED_RVALUE(%rsp), %rax
	ret

	.align 2
.Lld_float:
	movss	ffi_closure_RED_RVALUE(%rsp), %xmm0
	ret
	.align 2
.Lld_double:
	movsd	ffi_closure_RED_RVALUE(%rsp), %xmm0
	ret
	.align 2
.Lld_ldouble:
	fldt	ffi_closure_RED_RVALUE(%rsp)
	ret

	.align 2
.Lld_struct:
	/* There are four possibilities here, %rax/%rdx, %xmm0/%rax,
	   %rax/%xmm0, %xmm0/%xmm1.  We collapse two by always loading
	   both rdx and xmm1 with the second word.  For the remaining,
	   bit 8 set means xmm0 gets the second word, and bit 9 means
	   that rax gets the second word.  */
	movq	ffi_closure_RED_RVALUE(%rsp), %rcx
	movq	ffi_closure_RED_RVALUE+8(%rsp), %rdx
	movq	ffi_closure_RED_RVALUE+8(%rsp), %xmm1
	testl	$0x100, %eax
	cmovnz	%rdx, %rcx
	movd	%rcx, %xmm0
	testl	$0x200, %eax
	movq	ffi_closure_RED_RVALUE(%rsp), %rax
	cmovnz	%rdx, %rax
	ret

	cfi_endproc
	.size	ffi_closure_unix64,.-ffi_closure_unix64

	.align	2
	.globl	ffi_go_closure_unix64_sse
	.type	ffi_go_closure_unix64_sse,@function
	FFI_HIDDEN(ffi_go_closure_unix64_sse)

ffi_go_closure_unix64_sse:
	cfi_startproc
	subq	$ffi_closure_FS, %rsp
	cfi_adjust_cfa_offset(ffi_closure_FS)

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
	cfi_adjust_cfa_offset(ffi_closure_FS)
0:
	movq	%rdi, ffi_closure_OFS_G+0x00(%rsp)
	movq    %rsi, ffi_closure_OFS_G+0x08(%rsp)
	movq    %rdx, ffi_closure_OFS_G+0x10(%rsp)
	movq    %rcx, ffi_closure_OFS_G+0x18(%rsp)
	movq    %r8,  ffi_closure_OFS_G+0x20(%rsp)
	movq    %r9,  ffi_closure_OFS_G+0x28(%rsp)

	movq	8(%r10), %rdi		/* Load cif */
	movq	16(%r10), %rsi		/* Load fun */
	movq	%r10, %rdx		/* Load closure (user_data) */
	jmp	.Ldo_closure

	cfi_endproc
	.size	ffi_go_closure_unix64,.-ffi_go_closure_unix64

#endif /* __x86_64__ */
#if defined __ELF__ && defined __linux__
	.section	.note.GNU-stack,"",@progbits
#endif

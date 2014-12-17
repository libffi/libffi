/* -----------------------------------------------------------------------
   sysv.S - Copyright (c) 2000 Software AG
            Copyright (c) 2008 Red Hat, Inc.
 
   S390 Foreign Function Interface
 
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

#ifndef __s390x__
 
.text

	# r2:	cif->bytes
	# r3:	&ecif
	# r4:	ffi_prep_args
	# r5:	ret_type
	# r6:	ecif.rvalue
	# ov:	fn 
	# ov+8:	closure

	# This assumes we are using gas.
	.globl	ffi_call_SYSV
	FFI_HIDDEN(ffi_call_SYSV)
	.type	ffi_call_SYSV,%function
ffi_call_SYSV:
	.cfi_startproc
	stm	%r6,%r15,24(%r15)		# Save registers
	.cfi_offset r6, -72
	.cfi_offset r7, -68
	.cfi_offset r8, -64
	.cfi_offset r9, -60
	.cfi_offset r10, -56
	.cfi_offset r11, -52
	.cfi_offset r12, -48
	.cfi_offset r13, -44
	.cfi_offset r14, -40
	.cfi_offset r15, -36
	basr	%r13,0				# Set up base register
.Lbase:
	lr	%r11,%r15			# Set up frame pointer
	.cfi_def_cfa_register r11
	sr	%r15,%r2
	ahi	%r15,-96-48			# Allocate stack
	lr	%r8,%r6				# Save ecif.rvalue
	sr	%r9,%r9
	ic	%r9,.Ltable-.Lbase(%r13,%r5)	# Load epilog address
	l	%r7,96(%r11)			# Load function address
	st	%r11,0(%r15)			# Set up back chain
	ahi	%r11,-48			# Register save area
	.cfi_adjust_cfa_offset 48

	la	%r2,96(%r15)			# Save area
						# r3 already holds &ecif
	basr	%r14,%r4			# Call ffi_prep_args

	l	%r0,96+48+4(%r11)		# Go closure -> static chain
	lm	%r2,%r6,0(%r11)			# Load arguments
	ld	%f0,32(%r11)
	ld	%f2,40(%r11)
	la	%r14,0(%r13,%r9)		# Set return address
	br	%r7				# ... and call function

.LretNone:					# Return void
	l	%r4,48+56(%r11)
	lm	%r6,%r15,48+24(%r11)
	.cfi_remember_state
	.cfi_restore 15
	.cfi_restore 14
	.cfi_restore 13
	.cfi_restore 12
	.cfi_restore 11
	.cfi_restore 10
	.cfi_restore 9
	.cfi_restore 8
	.cfi_restore 7
	.cfi_restore 6
	.cfi_def_cfa r15, 96
	br	%r4
	.cfi_restore_state
	# This nopr is necessary so that the .cfi instructions between the br
	# above and the label below get executed.  See execute_cfa_program() in
	# the Gcc source code, libgcc/unwind-dw2.c.
	nopr

.LretFloat:
	l	%r4,48+56(%r11)
	ste	%f0,0(%r8)			# Return float
	lm	%r6,%r15,48+24(%r11)
	.cfi_remember_state
	.cfi_restore 15
	.cfi_restore 14
	.cfi_restore 13
	.cfi_restore 12
	.cfi_restore 11
	.cfi_restore 10
	.cfi_restore 9
	.cfi_restore 8
	.cfi_restore 7
	.cfi_restore 6
	.cfi_def_cfa r15, 96
	br	%r4
	.cfi_restore_state
	# See comment on the nopr above.
	nopr
 
.LretDouble:
	l	%r4,48+56(%r11)
	std	%f0,0(%r8)			# Return double
	lm	%r6,%r15,48+24(%r11)
	.cfi_remember_state
	.cfi_restore 15
	.cfi_restore 14
	.cfi_restore 13
	.cfi_restore 12
	.cfi_restore 11
	.cfi_restore 10
	.cfi_restore 9
	.cfi_restore 8
	.cfi_restore 7
	.cfi_restore 6
	.cfi_def_cfa r15, 96
	br	%r4
	.cfi_restore_state
	# See comment on the nopr above.
	nopr

.LretInt32:
	l	%r4,48+56(%r11)
	st	%r2,0(%r8)			# Return int
	lm	%r6,%r15,48+24(%r11)
	.cfi_remember_state
	.cfi_restore 15
	.cfi_restore 14
	.cfi_restore 13
	.cfi_restore 12
	.cfi_restore 11
	.cfi_restore 10
	.cfi_restore 9
	.cfi_restore 8
	.cfi_restore 7
	.cfi_restore 6
	.cfi_def_cfa r15, 96
	br	%r4
	.cfi_restore_state
	# See comment on the nopr above.
	nopr
 
.LretInt64:
	l	%r4,48+56(%r11)
	stm	%r2,%r3,0(%r8)			# Return long long
	lm	%r6,%r15,48+24(%r11)
	.cfi_remember_state
	.cfi_restore 15
	.cfi_restore 14
	.cfi_restore 13
	.cfi_restore 12
	.cfi_restore 11
	.cfi_restore 10
	.cfi_restore 9
	.cfi_restore 8
	.cfi_restore 7
	.cfi_restore 6
	.cfi_def_cfa r15, 96
	br	%r4
	.cfi_endproc
 
.Ltable:
	.byte	.LretNone-.Lbase		# FFI390_RET_VOID
	.byte	.LretNone-.Lbase		# FFI390_RET_STRUCT
	.byte	.LretFloat-.Lbase		# FFI390_RET_FLOAT
	.byte	.LretDouble-.Lbase		# FFI390_RET_DOUBLE
	.byte	.LretInt32-.Lbase		# FFI390_RET_INT32
	.byte	.LretInt64-.Lbase		# FFI390_RET_INT64

.ffi_call_SYSV_end:
	.size	 ffi_call_SYSV,.ffi_call_SYSV_end-ffi_call_SYSV


	.globl	ffi_closure_SYSV
	FFI_HIDDEN(ffi_closure_SYSV)
	.type	ffi_closure_SYSV,%function
ffi_closure_SYSV:
	.cfi_startproc
	stm	%r2,%r6,8(%r15)			# Save arguments
	.cfi_offset r6, -72
	lr	%r4,%r0				# Closure
	l	%r2,16(%r4)			#   ->cif
	l	%r3,20(%r4)			#   ->fun
	l	%r4,24(%r4)			#   ->user_data
.Ldoclosure:
	stm	%r12,%r15,48(%r15)		# Save registers
	.cfi_offset r12, -48
	.cfi_offset r13, -44
	.cfi_offset r14, -40
	.cfi_offset r15, -36
	basr	%r13,0				# Set up base register
.Lcbase:
	std	%f0,64(%r15)
	std	%f2,72(%r15)
	lr	%r1,%r15			# Set up stack frame
	ahi	%r15,-104
	.cfi_adjust_cfa_offset 104
	l	%r12,.Lchelper-.Lcbase(%r13)	# Get helper function
	la	%r5,96(%r1)
	st	%r5,96(%r15)			# Overflow
	la	%r5,8(%r1)			# GPRs
	la	%r6,64(%r1)			# FPRs
	st	%r1,0(%r15)			# Set up back chain

	bas	%r14,0(%r12,%r13)		# Call helper

	l	%r4,104+56(%r15)
	ld	%f0,104+64(%r15)		# Load return registers
	lm	%r2,%r3,104+8(%r15)
	l	%r6,104+24(%r15)		# Restore saved registers
	.cfi_restore r6
	lm	%r12,%r15,104+48(%r15)
	.cfi_adjust_cfa_offset -104
	.cfi_restore r12
	.cfi_restore r13
	.cfi_restore r14
	.cfi_restore r15
	br	%r4
	.cfi_endproc

	.align 4
.Lchelper:
	.long	ffi_closure_helper_SYSV-.Lcbase


.ffi_closure_SYSV_end:
	.size	 ffi_closure_SYSV,.ffi_closure_SYSV_end-ffi_closure_SYSV


	.globl	ffi_go_closure_SYSV
	FFI_HIDDEN(ffi_go_closure_SYSV)
	.type	ffi_go_closure_SYSV,%function
ffi_go_closure_SYSV:
	.cfi_startproc
	stm	%r2,%r6,8(%r15)			# Save arguments
	.cfi_offset r6, -72
	lr	%r4,%r0				# Load closure -> user_data
	l	%r2,4(%r4)			#   ->cif
	l	%r3,8(%r4)			#   ->fun
	j	.Ldoclosure
	.cfi_endproc

#else
 
.text
 
	# r2:	cif->bytes
	# r3:	&ecif
	# r4:	ffi_prep_args
	# r5:	ret_type
	# r6:	ecif.rvalue
	# ov:	fn 
	# ov+8:	closure

	# This assumes we are using gas.
	.globl	ffi_call_SYSV
	FFI_HIDDEN(ffi_call_SYSV)
	.type	ffi_call_SYSV,%function
ffi_call_SYSV:
	.cfi_startproc
	stmg	%r6,%r15,48(%r15)		# Save registers
	.cfi_offset r6, -112
	.cfi_offset r7, -104
	.cfi_offset r8, -96
	.cfi_offset r9, -88
	.cfi_offset r10, -80
	.cfi_offset r11, -72
	.cfi_offset r12, -64
	.cfi_offset r13, -56
	.cfi_offset r14, -48
	.cfi_offset r15, -40
	larl	%r13,.Lbase			# Set up base register
	lgr	%r11,%r15			# Set up frame pointer
	.cfi_def_cfa_register r11
	sgr	%r15,%r2
	aghi	%r15,-160-80			# Allocate stack
	lgr	%r8,%r6				# Save ecif.rvalue
	llgc	%r9,.Ltable-.Lbase(%r13,%r5)	# Load epilog address
	lg	%r7,160(%r11)			# Load function address
	stg	%r11,0(%r15)			# Set up back chain
	aghi	%r11,-80			# Register save area
	.cfi_adjust_cfa_offset 80

	la	%r2,160(%r15)			# Save area
						# r3 already holds &ecif
	basr	%r14,%r4			# Call ffi_prep_args

	lg	%r0,160+80+8(%r11)		# Go closure -> static chain
	lmg	%r2,%r6,0(%r11)			# Load arguments
	ld	%f0,48(%r11)
	ld	%f2,56(%r11)
	ld	%f4,64(%r11)
	ld	%f6,72(%r11)
	la	%r14,0(%r13,%r9)		# Set return address
	br	%r7				# ... and call function

.Lbase:
.LretNone:					# Return void
	lg	%r4,80+112(%r11)
	lmg	%r6,%r15,80+48(%r11)
	.cfi_remember_state
	.cfi_restore r15
	.cfi_restore r14
	.cfi_restore r13
	.cfi_restore r12
	.cfi_restore r11
	.cfi_restore r10
	.cfi_restore r9
	.cfi_restore r8
	.cfi_restore r7
	.cfi_restore r6
	.cfi_def_cfa r15, 160
	br	%r4
	.cfi_restore_state
	# This nopr is necessary so that the .cfi instructions between the br
	# above and the label below get executed.  See execute_cfa_program() in
	# the Gcc source code, libgcc/unwind-dw2.c.
	nopr

.LretFloat:
	lg	%r4,80+112(%r11)
	ste	%f0,0(%r8)			# Return float
	lmg	%r6,%r15,80+48(%r11)
	.cfi_remember_state
	.cfi_restore r6
	.cfi_restore r7
	.cfi_restore r8
	.cfi_restore r9
	.cfi_restore r10
	.cfi_restore r11
	.cfi_restore r12
	.cfi_restore r13
	.cfi_restore r14
	.cfi_restore r15
	.cfi_def_cfa r15, 160
	br	%r4
	.cfi_restore_state
	# See comment on the nopr above.
	nopr
 
.LretDouble:
	lg	%r4,80+112(%r11)
	std	%f0,0(%r8)			# Return double
	lmg	%r6,%r15,80+48(%r11)
	.cfi_remember_state
	.cfi_restore r15
	.cfi_restore r14
	.cfi_restore r13
	.cfi_restore r12
	.cfi_restore r11
	.cfi_restore r10
	.cfi_restore r9
	.cfi_restore r8
	.cfi_restore r7
	.cfi_restore r6
	.cfi_def_cfa r15, 160
	br	%r4
	.cfi_restore_state
	# See comment on the nopr above.
	nopr

.LretInt64:
	lg	%r4,80+112(%r11)
	stg	%r2,0(%r8)			# Return long
	lmg	%r6,%r15,80+48(%r11)
	.cfi_restore r15
	.cfi_restore r14
	.cfi_restore r13
	.cfi_restore r12
	.cfi_restore r11
	.cfi_restore r10
	.cfi_restore r9
	.cfi_restore r8
	.cfi_restore r7
	.cfi_restore r6
	.cfi_def_cfa r15, 160
	br	%r4
	.cfi_endproc
 
.Ltable:
	.byte	.LretNone-.Lbase		# FFI390_RET_VOID
	.byte	.LretNone-.Lbase		# FFI390_RET_STRUCT
	.byte	.LretFloat-.Lbase		# FFI390_RET_FLOAT
	.byte	.LretDouble-.Lbase		# FFI390_RET_DOUBLE
	.byte	0				# int32 retval not supported
	.byte	.LretInt64-.Lbase		# FFI390_RET_INT64

.ffi_call_SYSV_end:
	.size	 ffi_call_SYSV,.ffi_call_SYSV_end-ffi_call_SYSV


	.globl	ffi_closure_SYSV
	FFI_HIDDEN(ffi_closure_SYSV)
	.type	ffi_closure_SYSV,%function
ffi_closure_SYSV:
	.cfi_startproc
	stmg	%r2,%r6,16(%r15)		# Save arguments
	.cfi_offset r6, -112
	lgr	%r4,%r0				# Load closure
	lg	%r2,32(%r4)			#   ->cif
	lg	%r3,40(%r4)			#   ->fun
	lg	%r4,48(%r4)			#   ->user_data
.Ldoclosure:
	stmg	%r14,%r15,112(%r15)		# Save registers
	.cfi_offset r14, -48
	.cfi_offset r15, -40
	std	%f0,128(%r15)			# Save arguments
	std	%f2,136(%r15)
	std	%f4,144(%r15)
	std	%f6,152(%r15)
	lgr	%r1,%r15			# Set up stack frame
	aghi	%r15,-168
	.cfi_adjust_cfa_offset 168
	la	%r5,160(%r1)
	stg	%r5,160(%r15)			# Overflow
	la	%r5,16(%r1)			# GPRs
	la	%r6,128(%r1)			# FPRs
	stg	%r1,0(%r15)			# Set up back chain

	brasl	%r14,ffi_closure_helper_SYSV	# Call helper

	ld	%f0,168+128(%r15)		# Load return registers
	lg	%r2,168+16(%r15)
	lg	%r6,168+48(%r15)		# Restore saved registers
	.cfi_restore r6
	lmg	%r14,%r15,168+112(%r15)
	.cfi_restore r14
	.cfi_restore r15
	.cfi_adjust_cfa_offset -168
	br	%r14
	.cfi_endproc

.ffi_closure_SYSV_end:
	.size	 ffi_closure_SYSV,.ffi_closure_SYSV_end-ffi_closure_SYSV

	
	.globl	ffi_go_closure_SYSV
	FFI_HIDDEN(ffi_go_closure_SYSV)
	.type	ffi_go_closure_SYSV,%function
ffi_go_closure_SYSV:
	.cfi_startproc
	stmg	%r2,%r6,16(%r15)		# Save arguments
	.cfi_offset r6, -112
	lgr	%r4,%r0				# Load closure -> user_data
	lg	%r2,8(%r4)			#   ->cif
	lg	%r3,16(%r4)			#   ->fun
	j	.Ldoclosure
	.cfi_endproc

.ffi_go_closure_SYSV_end:
	.size	 ffi_go_closure_SYSV,.ffi_go_closure_SYSV_end-ffi_go_closure_SYSV

#endif

#if defined __ELF__ && defined __linux__
	.section	.note.GNU-stack,"",@progbits
#endif

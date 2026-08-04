* Copyright IBM Corp. and others 2026
FFISYS CELQPRLG DSASIZE=DSASZ,PSECT=ASP

*===================================================================
* FFISYS - xplink call trampoline
*===================================================================
*
* PURPOSE
*   Prepare arguments per the z/OS XPLINK calling convention,
*   call the foreign function, then store the return value.
*
* INCOMING ARGUMENTS
*   r1/r2/r3 carry the first three arguments; args 4+ live in
*   the caller's outgoing argument area at r4+2176+(rounded DSA).
*   Offsets are relative to that arg-area base:
*
*     +0   r1    foreign_function_address
*     +8   r2    ecif          (extended_ffi_cif *)
*     +20  r3    cif->flags    (FFI390_RET_* constant)
*     +24  @(r4) ecif->rvalue  (return-value buffer pointer)
*     +36  @(r4) cif->bytes    (total arg-area bytes required)
*     +44  @(r4) cif->nargs    (total argument count)
*     +52  @(r4) cif->nfixedargs (fixed args before '...')
*     +56  @(r4) lstor         (alloca'd buffer, cif->bytes long)
*
* NOTE: r4 is the XPLINK stack pointer biased by 2048.
*       r4+2048 = start of the DSA (register save area).
*       r4+2176 = start of the outgoing argument area.
*
* GPR USAGE
*-------------------------------------------------------------------
*  r0   GPR arg-slot counter.  0=none consumed; increments to 3
*       as r1/r2/r3 are assigned to outgoing args.  When r0==3
*       all GPR slots are full; further args spill to the stack.
*  r1   Outgoing arg register 1 (first integer/pointer slot).
*       Also used as scratch during setup.
*  r2   Outgoing arg register 2 (second integer/pointer slot).
*  r3   Outgoing arg register 3 (third integer/pointer slot).
*  r4   XPLINK stack pointer (biased by 2048); constant.
*  r5   Base address of cif->arg_types pointer array.  Set once
*       from r14 after the initial LG; not changed in the loop.
*  r6   Byte offset into the lstor buffer (alloca'd by caller,
*       filled by ffi_prep_args_call).  Index in 0(r6,r13).
*       Re-purposed at CALL: loaded with the fn-descriptor ptr
*       so LMG 5,6,0(r6) fetches the descriptor words.
*  r7   Byte offset into the outgoing argument area (base
*       r4+2176) during the argument loop.  Advanced by 8 (or
*       the struct size) after each argument.
*       Re-purposed at CALL and return handlers: holds the
*       ecif->rvalue pointer (return-value buffer).
*       After BASR 7,6 also serves as the post-call base reg.
*  r8   Not used as a GPR in the argument loop.
*  r9   Argument loop iteration counter.  Loaded from cif->nargs;
*       decremented by BCT at CONT2 (once per argument).
*  r10  Byte offset into the ffi_type* array (base r5).
*       Advanced by 8 after each argument.
*  r11  Integer scratch: ffi_type->type codes, intermediate
*       addresses, argument values.
*       Distinct from fpr11 (see FPR section below).
*  r12  Variadic fence counter.  Loaded from cif->nfixedargs;
*       decremented at CONT.  When it reaches 0, r14 is forced
*       to 4 to route FLT/D through GPRs instead of FPRs.
*       Also scratch inside STRCTDISP (struct HFA detection);
*       safe because STRCTDISP is only reached from ARGLOOP
*       after CONT has already decremented r12.
*  r13  lstor base pointer during the argument loop; loaded from
*       the lstor pointer at arg-area offset +60.
*       Re-purposed at CALL: reset to 0 then loaded with
*       rtype->type for return-value dispatch.
*  r14  FPR arg-slot counter.  0=none consumed; +1 per float/
*       double, +2 per long double.  When r14==4 all four FPR
*       slots (fpr0/2/4/6) are full; further FP args spill.
*       Forced to 4 at the variadic fence.
*  r15  Scratch: dispatch address, table base, and remaining
*       byte count in the struct copy loop (STRCTLP).
*
* FPR USAGE
*-------------------------------------------------------------------
*  fpr0   Outgoing FP arg slot 0; return value for float/double
*         and low half of long double return.
*  fpr2   Outgoing FP arg slot 1; imaginary part of complex
*         float/double return; high half of long double return.
*  fpr4   Outgoing FP arg slot 2.
*  fpr6   Outgoing FP arg slot 3.
*  fpr8   Scratch for the high-word half of a long-double pair
*         (DFPR0, DFPR4, DARGF, STFPCHKLD*); stored via
*         STD 8,8(r7,r15).
*  fpr11  Float/double staging register.  Loaded via LE/LD/
*         LER/LDR then written to the arg area with STE/STD.
*         Distinct from GPR 11.
*-------------------------------------------------------------------
*
         USING  CEEDSAHP,4

*Store argument registers on the stack
         STG 1,(2176+(((DSASZ+31)/32)*32))(,4)
         STG 2,(2176+(((DSASZ+31)/32)*32)+8)(,4)
         STG 3,(2176+(((DSASZ+31)/32)*32)+16)(,4)

         LG 14,0(,2)           ecif->cif
         LG 14,8(,14)          cif->arg_types
         STMG 0,15,SAVREGS     save live GPRs across helper calls
         STD 0,SAVFPRS
         STD 2,SAVFPRS+8
         STD 4,SAVFPRS+16
         STD 6,SAVFPRS+24
         STD 8,SAVFPRS+32
         STD 9,SAVFPRS+40
         STD 10,SAVFPRS+48
         STD 11,SAVFPRS+56
         STD 12,SAVFPRS+64
         STD 13,SAVFPRS+72
         STD 14,SAVFPRS+80
         STD 15,SAVFPRS+88
*What: Storing arguments in this routine's
*      local storage for future use
*Name:    ffi_prep_args_call
*Input:   stack, extended_cif
*Output:  void
*Action:  Is an external call to C-XPLINK routine
*         It saves function's arguments in this 
*         routine's local storage
         LG 1,(2176+(((DSASZ+31)/32)*32)+56)(,4)  lstor ptr
         LGR 13,1
         CELQCALL   PREPARGS,WORKREG=10

         LGR 5,14              Copy of parameter types
*Reset registers used in code gen
*         SR  0,0              GPR counter
          LA  0,0
*         SR  14,14            FPR counter
          LA  14,0
*         SR  7,7              Offset in the arg area
          LA  7,0
*         SR  10,10            Offset in array of parm types
          LA  10,0
*         SR  6,6              Offset in stored parm values
          LA  6,0

*Dumb handling for now, but if struct return with size > 24
*bytes, we need to allocate space for the dummy argument
*that holds the return value pointer.
*Skip for: FFI_TYPE_COMPLEX (15) - always returns in FPR/GPR pairs
*          anything with size <= 24 - returns in GPR1/2/3
         LG 15,(2176+(((DSASZ+31)/32)*32)+8)(,4)
         LG 15,0(,15)           ecif->cif
         LG 15,16(,15)          cif->rtype
         LLGH 11,10(,15)        rtype->type
         LG 15,0(,15)           rtype->size
         CFI 11,15              FFI_TYPE_COMPLEX?
         BE GETNARGS            yes: returns in regs, no hidden ptr
         CFI 15,24
         BNH GETNARGS

         LG 1,(2176+(((DSASZ+31)/32)*32)+24)(,4)         
         AHI 0,1                One less gpr to work with
         AHI 7,8                argument area is 8 bytes larger

GETNARGS DS 0H
*Get the cif->nargs from caller's stack
         LGF 9,(2176+(((DSASZ+31)/32)*32)+44)(,4)
*Get the cif->nfixedargs countdown into r12
*r12 counts down from nfixedargs to 0; when it hits 0 we
*force r14=4 (FPR counter exhausted) so FLT/D go via GPR path
         LGF 12,(2176+(((DSASZ+31)/32)*32)+52)(,4)
*         CGIJE 9,0,CALL
         CFI 9,0
         BE CALL

*Place arguments passed to the foreign function based on type
ARGLOOP  LG  11,0(10,5)       Get pointer to current ffi_type
         LLGC 11,11(11)       ffi_type->type
         SLL 11,2             Find offset in branch tabel
         LA  15,ATABLE
         L   15,0(11,15)
         AFI 7,7
         N 7,=X'FFFFFFF8'
         BR  15
 
*Following code prepares ffi arguments, according to xplink

* technically, this isn't allowed?
* but openJ9 uses ffi_type_void for void functions
* so support it since it doesn't break anything
* assuming we don't have something weird like a void type
* followed by real parameters
VOID     DS  0H               ffi_type_void
         B CALL

UI32     DS 0H                ffi_type_uint32
         LA 15,U32
         LR 11,0
         SLL 11,2             Pass this parm in gpr or arg area
         L  15,0(11,15)
         CFI  0,3             GPRs left to pass parms in?
         BL INC0
         LA 0,3               Reached max gprs
         B  J0
INC0     DS 0H
         AHI 0,1              Now we have one less gpr left
J0       DS 0H
         BR 15

UGPR1    DS 0H                UI32 type passed in gpr1
         LLGF 1,0(6,13)
         AHI 7,8              Advance to next word in arg area
         AHI 6,4              Advance to next word in parm value
         B CONT               Next parameter
UGPR2    DS 0H                UI32 type passed in gpr2
         LLGF 2,0(6,13)
         AHI 7,8              Advance to next word in arg area
         AHI 6,4              Advance to next word in parm value
         B CONT               Next parameter
UGPR3    DS 0H                UI32 type passed in gpr3
         LLGF 3,0(6,13)
         AHI 7,8              Advance to next word in arg area
         AHI 6,4              Advance to next word in parm value
         B CONT               Next parameter
UARGA    DS 0H                UI32 stored in arg area
         LLGF 11,0(6,13)        Argument value
         LA 15,2176(,4)       Start of arg area
         STG 11,0(7,15)       Store in next word in arg area
         AHI 7,8              Bump to the next word in arg area
         AHI 6,4              Advance to next word in parm value
         B CONT               Next parameter

I        DS  0H               ffi_type_int
SI32     DS 0H                ffi_type_sint32
         LA 15,I32
         LR 11,0
         SLL 11,2             Pass this parm in gpr or arg area
         L  15,0(11,15)
         CFI  0,3             GPRs left to pass parms in?
         BL INC
         LA 0,3               Reached max gprs
         B  J
INC      DS 0H
         AHI 0,1              Now we have one less gpr left
J        DS 0H
         BR 15

IGPR1    DS 0H                INT/SI32 type passed in gpr1
         LGF 1,0(6,13)
         AHI 7,8              Advance to next word in arg area
         AHI 6,4              Advance to next word in parm value
         B CONT               Next parameter
IGPR2    DS 0H                INT/SI32 type passed in gpr2
         LGF 2,0(6,13)
         AHI 7,8              Advance to next word in arg area
         AHI 6,4              Advance to next word in parm value
         B CONT               Next parameter
IGPR3    DS 0H                INT/SI32 type passed in gpr3
         LGF 3,0(6,13)
         AHI 7,8              Advance to next word in arg area
         AHI 6,4              Advance to next word in parm value
         B CONT               Next parameter
IARGA    DS 0H                INT/SI32 stored in arg area
         LGF 11,0(6,13)       Argument value
         LA 15,2176(,4)       Start of arg area
         STG 11,0(7,15)       Store in next word in arg area
         AHI 7,8              Bump to the next word in arg area
         AHI 6,4              Advance to next word in parm value
         B CONT               Next parameter

UI8      DS 0H                ffi_type_uint8
         LA 15,U8
         LR 11,0
         SLL 11,2             Pass this parm in gpr or arg area
         L  15,0(11,15)
         CFI  0,3             GPRs left to pass parms in?
         BL INC22
         LA 0,3               Reached max gprs
         B  J22
INC22    DS 0H
         AHI 0,1              Now we have one less gpr left
J22      DS 0H
         BR 15 

U8GPR1   DS 0H                Char type passed in gpr1
         LLGC 1,0(6,13)
         AHI 7,8              Advance to next word in arg area
         AHI 6,1              Advance the first byte of parm value
         B  CONT              Next parameter
U8GPR2   DS 0H                Char type passed in gpr2
         LLGC 2,0(6,13)
         AHI 7,8              Advance to next word in arg area
         AHI 6,1              Advance the first byte of parm value
         B CONT               Next parameter
U8GPR3   DS 0H                Char type passed in gpr3
         LLGC 3,0(6,13)   
         AHI 7,8              Advance to the next word in arg area
         AHI 6,1              Advance the first byte of parm value
         B CONT
U8ARGA   DS 0H                Char stored in arg area
         LLGC 11,0(6,13)      Argument value
         LA 15,2176(,4)       Start of arg area
         STG 11,0(7,15)       Store in next word in arg area
         AHI 7,8              Bump to the next word in arg area
         AHI 6,1              Advance the first byte of parm value
         B CONT               Next parameter

SI8      DS 0H                ffi_type_sint8 
         LA 15,I8
         LR 11,0
         SLL 11,2             Pass this parm in gpr or arg area
         L  15,0(11,15)
         CFI  0,3             GPRs left to pass parms in?
         BL INC2
         LA 0,3               Reached max gprs
         B  J2
INC2     DS 0H
         AHI 0,1              Now we have one less gpr left
J2       DS 0H
         BR 15 

I8GPR1   DS 0H                Char type passed in gpr1
         LGB 1,0(6,13)
         AHI 7,8              Advance to next word in arg area
         AHI 6,1              Advance the first byte of parm value
         B  CONT              Next parameter
I8GPR2   DS 0H                Char type passed in gpr2
         LGB 2,0(6,13)
         AHI 7,8              Advance to next word in arg area
         AHI 6,1              Advance the first byte of parm value
         B CONT               Next parameter
I8GPR3   DS 0H                Char type passed in gpr3
         LGB 3,0(6,13)   
         AHI 7,8              Advance to the next word in arg area
         AHI 6,1              Advance the first byte of parm value
         B CONT
I8ARGA   DS 0H                Char stored in arg area
         LGB 11,0(6,13)       Argument value
         LA 15,2176(,4)       Start of arg area
         STG 11,0(7,15)       Store in next word in arg area
         AHI 7,8              Bump to the next word in arg area
         AHI 6,1              Advance the first byte of parm value
         B CONT               Next parameter

UI16     DS 0H                ffi_type_uint16
         LA 15,U16
         LR 11,0
         SLL 11,2
         L  15,0(11,15)       Pass this parm in gpr or arg area
         CFI  0,3             GPRs left to pass parms in?
         BL INC3
         LA 0,3               Reached max gprs
         B  J3
INC3     DS 0H
         AHI 0,1              Now we have one less gpr left
J3       DS 0H
         BR 15 

U16GPR1  DS 0H                u_short type passed in gpr1
         LLGH 1,0(6,13)
         AHI 7,8              Advance to the next word in arg area
         AHI 6,2              Advance the first 2 bytes of parm value
         B CONT               Next parameter
U16GPR2  DS 0H                u_short type passed in gpr2
         LLGH 2,0(6,13)
         AHI 7,8              Advance to the next word in arg area
         AHI 6,2              Advance the first 2 bytes of parm value
         B CONT               Next parameter
U16GPR3  DS 0H                u_short passed in gpr3
         LLGH 3,0(6,13)
         AHI 7,8              Advance to the next word in arg area
         AHI 6,2              Advance the first 2 bytes of parm value
         B CONT               Next parameter
U16ARGA  DS 0H                u_short in arg area
         LLGH 11,0(6,13)       Argument value
         LA 15,2176(,4)       Start of arg area
         STG 11,0(7,15)       Store in next word in arg area
         AHI 7,8              Bump to the next word in arg area
         AHI 6,2              Advance the first 2 bytes of parm value
         B CONT               Next parameter

SI16     DS 0H                ffi_type_sint16
         LA 15,S16
         LR 11,0
         SLL 11,2            Pass this parm in gpr or arg area
         L  15,0(11,15)      
         CFI  0,3             GPRs left to pass parms in?
         BL INC4
         LA 0,3               Reached max gprs
         B  J4            
INC4     DS 0H
         AHI 0,1              Now we have one less gpr left
J4       DS 0H
         BR 15 

S16GPR1  DS 0H                s_SHORT type passsed in gpr1
         LGH 1,0(6,13)   
         AHI 7,8              Advance to the next word in arg area
         AHI 6,2              Advance the first 2 bytes of parm value
         B CONT               Next parameter
S16GPR2  DS 0H                s_SHORT type passed in gpr2
         LGH 2,0(6,13)
         AHI 7,8              Advance to the next word in arg area
         AHI 6,2              Advance the first 2 bytes of parm value
         B CONT               Next parameter
S16GPR3  DS 0H                s_SHORT type passed in gpr3
         LGH 3,0(6,13)
         AHI 7,8              Advance to next word in arg area
         AHI 6,2              Advance the first 2 bytes of parm value
         B CONT               Next parameter
S16ARGA  DS 0H                s_SHORT in arg area
         LGH 11,0(6,13)        Argument value
         LA 15,2176(,4)       Start of arg area
         STG 11,0(7,15)       Store in next word in arg area
         AHI 7,8              Bump to the next word in arg area
         AHI 6,2              Advance the first 2 bytes of parm value
         B CONT               Next parameter

SI64     DS 0H                ffi_type_sint64
         LA 15,S64
         LR 11,0
         SLL 11,2             Pass this parm in gpr or arg area
         L  15,0(11,15)
         CFI  0,3             GPRs left to pass parms in?
         BL INC5
         LA 0,3               Reached max gprs
         B  J5
INC5     DS 0H
         AHI 0,1              Now we have one less gpr left
J5       DS 0H
         BR 15 

S64GPR1 DS 0H                SINT64 type passed in gpr1
         LG  1,0(6,13)
         AHI 7,8              Advance one slot in arg area
         AHI 6,8              Advance the first 8 bytes of parm value
         B CONT               Next parameter
S64GPR2 DS 0H                SINT64 type passed in gpr2
         LG  2,0(6,13)
         AHI 7,8              Advance one slot in arg area
         AHI 6,8              Advance the first 8 bytes of parm value
         B CONT               Next parameter
S64GPR3  DS 0H                SINT64 type passed in gpr3
         LG  3,0(6,13)
         AHI 7,8              Advance one slot in arg area
         AHI 6,8              Advance the first 8 bytes of parm value
         B CONT               Next parameter
S64ARGA  DS 0H                SINT64 in arg area
         LG  11,0(6,13)        Argument value
         LA 15,2176(,4)       Start of arg area
         STG  11,0(7,15)       Store in next word in arg area
         AHI 7,8              Bump one word in arg area
         AHI 6,8              Advance the first 8 bytes of parm value
         B CONT               Next parameter

PTR      DS 0H                ffi_type_pointer
         LA 15,PTRS
         LR 11,0
         SLL 11,2             Pass this parm in gpr or arg area
         L  15,0(11,15)      
         CFI  0,3             GPRs left to pass parms in?
         BL INC6
         LA 0,3               Reached max gprs
         B  J6
INC6     DS 0H
         AHI 0,1              Now we have one less gpr left
J6       DS 0H
         BR 15 

PTRG1    DS 0H                PTR type passed in gpr1
         LG 1,0(6,13)
         AHI 7,8              Advance to the next word in arg area
         AHI 6,8
         B CONT               Next parameter
PTRG2    DS 0H                PTR type passed in gpr2
         LG 2,0(6,13)
         AHI 7,8              Advance to the next word in arg area
         AHI 6,8
         B CONT               Next parameter
PTRG3    DS 0H                PTR type passed in gpr3
         LG 3,0(6,13)
         AHI 7,8              Advance to the next word in arg area
         AHI 6,8
         B CONT               Next parameter
PTRARG   DS 0H                PTR in arg area
         LG 11,0(6,13)         Argument value
         LA 15,2176(,4)       Start of arg area
         STG 11,0(7,15)       Store in next word in arg area
         AHI 7,8              Bump to the next word in arg area
         AHI 6,8
         B CONT               Next parameter

UI64     DS 0H                ffi_type_uint64
         LA 15,UI64S
         LR 11,0
         SLL 11,2             Pass this parm in gpr or arg area
         L 15,0(11,15)
         CFI 0,3              GPRs left to pass parms in?
         BL INC64
         LA 0,3               Reached max gprs
         B J64
INC64    DS 0H
         AHI 0,1
J64      DS 0H
         BR 15

U64GP1   DS 0H                u_INT64 passed in gpr1
         LG 1,0(6,13)
         AHI 7,8              Advance one slot in arg area
         AHI 6,8              Advance the first 8 bytes of parm value
         B CONT               Next parameter
U64GP2   DS 0H                u_INT64 passed in gpr2
         LG 2,0(6,13)
         AHI 7,8              Advance one slot in arg area
         AHI 6,8              Advance the first 8 bytes of parm value
         B CONT               Next parameter
U64GP3   DS 0H                u_INT64 passed in gpr3
         LG 3,0(6,13)
         AHI 7,8              Advance one slot in arg area
         AHI 6,8              Advance the first 8 bytes of parm value
         B CONT               Next parameter
U64ARGA  DS 0H                u_INT64 in arg area
         LG 11,0(6,13)      Argument value
         LA 15,2176(,4)       Start of arg area
         STG  11,0(7,15)       Store in next word in arg area
         AHI 7,8              Bump one word in arg area
         AHI 6,8              Advance the first 8 bytes of parm value
         B CONT               Next parameter
   
FLT      DS 0H                ffi_type_float
* for variadics 12 is number of fixed args left
* if it's zero we just use GPRs/memory no FPRs
* branch to the unsigned int32 label for that 
         CFI  12,0
         BE   UI32
         LA 15,FLTS
         LR 11,14
         SLL 11,2             Pass in fpr or arg area
         L 15,0(11,15)
         CFI 14,4             FPRs left to pass parms in?
         BL INC7L
         LA 14,4              Reached max fprs
         B JL7
INC7L    DS 0H
         AHI 14,1             Now we have one less fpr left
         CFI 0,3
         BL INCGF
         LA 0,3
         B JL7
INCGF    DS 0H
         AHI 0,1
JL7      DS 0H
         BR 15

FLTR0    DS 0H                FLOAT passed in fpr0
         LE 0,0(6,13)
         LER 11,0
         B FLTSTR
FLTR2    DS 0H                FLOAT passed in fpr2
         LE 2,0(6,13)
         LER 11,2
         B FLTSTR
FLTR4    DS 0H                FLOAT passed in fpr4
         LE 4,0(6,13)
         LER 11,4
         B FLTSTR
FLTR6    DS 0H                FLOAT passed in fpr6
         LE 6,0(6,13)
         LER 11,6
         B FLTSTR
ARGAFT   DS 0H                FLOAT in arg area
         LE 11,0(6,13)        Argument value
FLTSTR   DS 0H
         LA 15,2176(,4)       Start of arg area
         STE 11,0(7,15)       Store in next word in arg area
         AHI 7,8              Bump to the next word in arg area
         AHI 6,4              Advance the first 4 bytes of parm value
         B CONT               Next parameter

D        DS 0H                ffi_type_double
* for variadics 12 is number of fixed args left
* if it's zero we just use GPRs/memory no FPRs
* branch to the unsigned int64 label for that 
         CFI  12,0
         BE   UI64
DBLNVP   DS   0H
         LA 15,FLD
         LR 11,14
         SLL 11,2             Pass in fpr or arg area
         L  15,0(11,15)
         CFI  14,4            FPRs left to pass parms in?
         BL INC7
         LA 14,4              Reached max fprs
         B  J7
INC7     DS 0H
         AHI 14,1             Now we have one less fpr left
         CFI 0,2              Reached max gprs
         BL INCGD
         LA 0,3
         B J7
INCGD    DS 0H
         AHI 0,1
J7       DS 0H
         BR 15 

FLDR0    DS 0H                DOUBLE passed in fpr0
         LD 0,0(6,13)
         LDR 11,0
         B DBLSTR
FLDR2    DS 0H                DOUBLE passed in fpr2
         LD 2,0(6,13)
         LDR 11,2
         B DBLSTR
FLDR4    DS 0H                DOUBLE passed in fpr4
         LD 4,0(6,13)
         LDR 11,4
         B DBLSTR
FLDR6    DS 0H                DOUBLE passed in fpr6
         LD 6,0(6,13)
         LDR 11,6
         B DBLSTR
ARGAFL   DS 0H                DOUBLE in arg area
         LD 11,0(6,13)        Argument value
DBLSTR   DS 0H
         LA 15,2176(,4)       Start of arg area
         STD 11,0(7,15)       Store in next two words in arg area
         AHI 7,8              Bump to the next two words in arg area
         AHI 6,8              Bump stack twice, cuz double is 8byte
         B CONT               Next parameter

LD       DS 0H                ffi_type_longdouble
* for variadics 12 is number of fixed args left
* if it's zero we just use GPRs/memory no FPRs
* branch to the 16 byte struct label for that
         CFI  12,0
         BNE  LDNVP
         LA   15,16           "struct" size 16
         B    BYTE16
LDNVP    DS   0H
         LA 15,LDD
         LR 11,14
         SLL 11,2             Pass in fpr or arg area
         L  15,0(11,15)
         CFI  14,3            fewer than 2 FPR pairs left?
         BL INC8
         LA 14,3              Reached max fprs (ldbl needs 2 slots)
         B  J8
INC8     DS 0H
         AHI 14,2             long double takes two FPR slots
         CFI 0,2              room for two more GPR shadow slots?
         BL INCGLLD
         LA 0,3
         B J8
INCGLLD  DS 0H                long double shadows two GPR slots
         AHI 0,2              so bump GPR counter by 2
J8       DS 0H
         BR 15

DFPR0    DS 0H                long double passed in fpr0+fpr2
         LD 0,0(6,13)
         LD 2,8(6,13)
         LDR 11,0
         LDR 8,2
         B LDBLSTR
DFPR4    DS 0H                long double passed in fpr4+fpr6
         LD 4,0(6,13)
         LD 6,8(6,13)
         LDR 11,4
         LDR 8,6
         LA 14,4              all FPR slots now in use
         LA 0,3               all GPR slots now in use
         B LDBLSTR
DARGF    DS 0H                long double spilled to arg area
         LD 11,0(6,13)        low half
         LD 8,8(6,13)         high half
LDBLSTR  DS 0H
         LA 15,2176(,4)       Start of arg area
         STD 11,0(7,15)       Store low half in arg area
         STD 8,8(7,15)        Store high half in arg area
         AHI 7,16             Bump two slots (16 bytes)
         AHI 6,16             Advance 16 bytes in local store
         B CONT               Next parameter


* CPXANY: called for FFI_TYPE_COMPLEX (type code 15).
* Reads elements[0]->type to dispatch to CPXF/CPXD/CPXLD.
* When r12==0 (past the variadic fixed-arg boundary) treat the
* whole complex value as an opaque struct in GPR/memory, same as
* FLT and D do when they redirect to UI32/UI64.
* Register state on entry is the same as for CPXF/CPXD/CPXLD.
CPXANY   DS 0H
         CFI  12,0                 past variadic fence?
         BE   STRCT                if yes, treat as struct (no FPRs)
         LG  11,0(10,5)            pointer to current ffi_type
         LG  11,16(,11)            ffi_type->elements ptr
         LG  11,0(,11)             elements[0] pointer
         LLGC 11,11(,11)           elements[0]->type (low byte)
         CFI  11,2                 FFI_TYPE_FLOAT?
         BE  CPXF
         CFI  11,3                 FFI_TYPE_DOUBLE?
         BE  CPXD
         CFI  11,4                 FFI_TYPE_LONGDOUBLE?
         BE  CPXLD
         B   STRCT                 non-float complex, treat as struct

CPXF     DS 0H
         CFI 14,4               no FPRs are available
         BNL STRCT              if yes, not special case

         LG  11,0(10,5)
         LG 15,0(,11)           type->size

         CFI 14,3               1 FPR is available
         BNL STFPCHKF26         use fpr6 + memory

         CFI 14,2               2 FPRs available
         BE STFPCHKF246         use fpr4 + fpr6

         CFI 14,1               3 FPRs available
         BE STFPCHKF224         use fpr2 + fpr4

         CFI 14,0               4 FPRs available
         BE STFPCHKF202         use fpr0 + fpr2

STFPCHKF26 DS 0H
         LE 6,0(6,13)           first float will go in fpr
         AFI 0,1                one less GPR available
         AFI 14,1               one less FPR available

         AFI 6,4                bump local store 4 bytes
         AFI 15,-4              we've handled 4 bytes
         AFI 7,4                advance 4 bytes into argarea

         B CPXMRG               copy rest of struct

STFPCHKF202 DS 0H
         LE 0,0(6,13)           first float
         LE 2,4(6,13)           second float
         AFI 0,2                two less GPRs available
         AFI 14,2               two less FPRs available

         AFI 6,8                bump arg area by 8 bytes
         AFI 15,-8              we've handled 8 bytes (all the struct)
         AFI 7,8                bump local storage by 8 bytes

         B CPXMRG

STFPCHKF224 DS 0H
         LE 2,0(6,13)           first float
         LE 4,4(6,13)           second float
         AFI 0,2                two less GPRs available
         AFI 14,2               two less FPRs available

         AFI 6,8                bump arg area by 8 bytes
         AFI 15,-8              we've handled 8 bytes (all the struct)
         AFI 7,8                bump local storage by 8 bytes

         B CPXMRG


STFPCHKF246 DS 0H
         LE 4,0(6,13)           first float
         LE 6,4(6,13)           second float
         AFI 0,2                two less GPRs available
         AFI 14,2               two less FPRs available

         AFI 6,8                bump arg area by 8 bytes
         AFI 15,-8              we've handled 8 bytes (all the struct)
         AFI 7,8                bump local storage by 8 bytes

         B CPXMRG

CPXD     DS 0H
         CFI 14,4               no FPRs are available
         BNL STRCT              if yes, not special case
         LG 11,0(10,5)          ensure we have size
         LG 15,0(,11)           ready for labels we branch to

         CFI 14,3               1 FPR is available
         BNL STFPCHKD26         use fpr6+memory

         CFI 14,2               2 FPRs are available
         BE STFPCHKD246         use fpr4+fpr6

         CFI 14,1               3 FPRs are available
         BE STFPCHKD224         use fpr2+fpr4

         CFI 14,0               4 FPRs are available
         BE STFPCHKD202         use fpr0+fpr2


STFPCHKD26 DS 0H
         LD 6,0(6,13)           first double
         AFI 0,1                one less GPR available
         AFI 14,1               one less FPRs available

         AFI 6,8
         AFI 15,-8
         AFI 7,8

         B CPXMRG

STFPCHKD202 DS 0H
         LD 0,0(6,13)           first double
         LD 2,8(6,13)           second double
         AFI 0,2                two less GPR available
         AFI 14,2               two less FPRs available

         AFI 6,16
         AFI 15,-16
         AFI 7,16

         B CPXMRG

STFPCHKD224 DS 0H
         LD 2,0(6,13)           first double
         LD 4,8(6,13)           second double
         AFI 0,2                two less GPR available
         AFI 14,2               two less FPRs available

         AFI 6,16
         AFI 15,-16
         AFI 7,16

         B CPXMRG


STFPCHKD246 DS 0H
         LD 4,0(6,13)           first double
         LD 6,8(6,13)           second double
         AFI 0,2                two less GPR available
         AFI 14,2               two less FPRs available

         AFI 6,16
         AFI 15,-16
         AFI 7,16

         B CPXMRG

CPXLD    DS 0H
         CFI 14,3               fewer than 2 FPR pairs left, cant fit
         BNL STRCT              fall through to struct path
         LG 11,0(10,5)          ensure we have size
         LG 15,0(,11)           ready for labels we branch to

         CFI 14,0               4 FPR slots free: fpr0+fpr2+fpr4+fpr6
         BE STFPCHKLD

         CFI 14,1               3 FPR slots free, use fpr4+fpr6+argarea
         BE STFPCHKLD1

         CFI 14,2               2 FPR slots free, use fpr4+fpr6+argarea
         BE STFPCHKLD1

STFPCHKLD DS 0H
         LD 0,0(6,13)           low half of first long double
         LD 2,8(6,13)           high half of first long double
         LD 4,16(6,13)          low half of second long double
         LD 6,24(6,13)          high half of second long double
         AFI 0,3                all 3 GPR shadow slots now used
         AFI 14,4               all 4 FPR slots now used

         AFI 6,32
         AFI 15,-32
         AFI 7,32

         B CPXMRG

STFPCHKLD1 DS 0H
         LD 4,0(6,13)           low half of first long double (fpr4)
         LD 6,8(6,13)           high half of first long double (fpr6)
         AFI 0,2                two GPR shadow slots used
         AFI 14,2               two FPR slots used

         AFI 6,16
         AFI 15,-16
         AFI 7,16

         B CPXMRG

*STRCTDISP: entry for FFI_TYPE_STRUCT (type 13) from ATABLE.
*Call STFPTYP (ffi_struct_float_pair_type) via CELQCALL.
*Save ALL GPRs (r0-r15) so no clobbered register is missed.
*r1 = ffi_type* loaded before r5 is replaced with LE env.
*Result in r3 moved to r11; then full restore via LMG.
STRCTDISP DS 0H
         STMG 0,15,SDALL        save all GPRs
* Store ffi_type* into STFARG while r5 still holds arg_types base.
* CELQCALL (STFARG) passes &STFARG in r1; C receives ffi_type**.
         LG  1,0(10,5)          r1 = current ffi_type*
         STG 1,STFARG           STFARG = ffi_type*
         LG  5,SAVREGS+40       r5 = LE env for CELQCALL
         CELQCALL STFPTYP,(STFARG),WORKREG=10
* Patch r11 slot in SDALL with result (r3) before LMG restores it.
* GPR 11 slot = SDALL + 11*8 = SDALL+88.
         STG 3,SDALL+88         r11 slot = STFPTYP result
         LMG 0,15,SDALL         restore all GPRs; r11 = result
* Dispatch on result.
         CFI 11,2               FFI_TYPE_FLOAT?
         BE  CPXF
         CFI 11,3               FFI_TYPE_DOUBLE?
         BE  CPXD
         CFI 11,4               FFI_TYPE_LONGDOUBLE?
         BE  CPXLD
* not an HFA - fall through to STRCT

*determine how to pass the struct based on size
*at this point we've weeded out all the float/double
*pair structs that get passed purely in FPRs
STRCT    DS 0H
         LG  11,0(10,5)
         LG  15,0(,11)          type->size
         CFI 15,8               Struct <= 8 bytes?
         BNH  BYTE8
         CFI 15,16              Struct <= 16 bytes?
         BNH   BYTE16
         CFI 15,24              Struct <= 24 bytes?
         B BYTE24

BYTE8    DS 0H               Struct <= 8 bytes

*Since a struct here can be <8 bytes
*we need to pad it to 8 bytes in the arg area
*since saves in the arg area always assume 8 bytes for register
*so even on a for example 1 byte struct, the arg area has 8 bytes
*this is true for all cases where
*max struct size < register storage used
*so <8 byte sturcts regardless of register storage used, will be padded
*similarly with <16 byte structs using 2 registers,
*and <24 byte structs
*using all 3 registers
*I assume this is because the first 24 bytes of the arg area are
*technically the save area for the first 3 registers
BYTE8R1  DS 0H               Struct goes in R1
         CFI 0,1             is R1 available?
         BNL BYTE8R2

         LG 1,0(6,13)

         AGR 6,15            Advance to next byte in arg area
         AFI 15,-8           If struct is <8 bytes arg area
         AFI 7,8             Needs padding to 8 bytes

         LA 0,1
         B STRCTLP

BYTE8R2  DS 0H               Struct goes in R2
         CFI 0,2             is R2 available?
         BNL BYTE8R3

         LG 2,0(6,13)

         AGR 6,15           Advance to next byte in arg area
         AFI 15,-8          If struct is <8 bytes arg area
         AFI 7,8            Needs padding to 8 bytes

         LA 0,2
         B STRCTLP


BYTE8R3  DS 0H               Struct goes in R3
         CFI 0,3             is R3 available?
         BNL STRCTLP

         LG 3,0(6,13)

         AGR 6,15           Advance to next byte in arg area
         AFI 15,-8          If struct is <8 bytes arg area
         AFI 7,8            Needs padding to 8 bytes

         LA 0,3
         B STRCTLP

BYTE8F0  DS 0H               Struct goes in FPR0
BYTE8F2  DS 0H               Struct goes in FPR2
BYTE8F4  DS 0H               Struct goes in FPR4
BYTE8F6  DS 0H               Struct goes in FPR6

BYTE16   DS 0H               Struct <= 16 bytes

BYTE16R1 DS 0H               Struct goes in R1-R2
         CFI 0,1             are R1+R2 available?
         BNL BYTE16R2
        
         LG 1,0(6,13)
         LG 2,8(6,13)

         AGR 6,15            Advance to next byte in arg area
         AFI 15,-16          If struct is <16 bytes arg area
         AFI 7,16            Needs padding to 16 bytes

         LA 0,2
         B STRCTLP


BYTE16R2 DS 0H               Struct goes in R2-R3
         CFI 0,2             are R2+R3 available?
         BNL BYTE16R3
         
         LG 2,0(6,13)
         LG 3,8(6,13)

         AGR 6,15            Advance to next byte in arg area
         AFI 15,-16          If struct is <16 bytes arg area
         AFI 7,16            Needs padding to 16 bytes

         LA 0,3
         B STRCTLP
         

BYTE16R3 DS 0H               Struct head in R3, remainder in memory
         CFI 0,3             is R3 available?
         BNL STRCTLP
         LG 3,0(6,13)

         LA 0,3 
         B STRCTLP

BYTE16F0 DS 0H               Struct goes in FPR0-FPR2
BYTE16F2 DS 0H               Struct goes in FPR2-FPR4
BYTE16F4 DS 0H               Struct goes in FPR4-FPR6


BYTE24   DS 0H               Struct <= 24 bytes


BYTE24R1 DS 0H               Struct goes in R1-R3
         CFI 0,1             are R1+R2+R3 available?
         BNL BYTE24R2 
         LG 1,0(6,13)
         LG 2,8(6,13)
         LG 3,16(6,13)

*        AGR 6,15            Advance to next byte in arg area
         AFI 6,24
         AFI 15,-24          If struct is <24 bytes arg area
         AFI 7,24            Needs padding to 24 bytes

         LA 0,3
         B STRCTLP

BYTE24R2 DS 0H               Struct goes in R2+R3+memory
         CFI 0,2             are R2+R3 available?
         BNL BYTE24R3 
         LG 2,0(6,13)
         LG 3,8(6,13)
        
         LA 0,3
         B STRCTLP

BYTE24R3 DS 0H               Struct goes in R3+memory
         CFI 0,3             is R3 available?
         BNL STRCTLP 
         LG 3,0(6,13)

         LA 0,3
         B STRCTLP

BYTE24F0 DS 0H               Struct goes in FPR0-FPR4+Memory
BYTE24F2 DS 0H               Struct goes in FPR2-FPR6+Memory
BYTE24F4 DS 0H               Struct goes in FPR4-FPR6+Memory
BYTE24F6 DS 0H               Struct goes in FPR6+Memory

CPXMRG   DS 0H               clamp GPR counter to 3 after FPR-split
         CFI 0,4
         BL STRCTLP
         LA 0,3

STRCTLP  DS 0H               Rest of struct goes in Memory
         CFI 15,0            Size remaining > 0?
         BNH CONT

         LB 11,0(6,13)        Load the byte of the struct
         STC 11,2176(7,4)     Store in next byte in arg area

         AFI  6,1            Move to next byte in struct
         AFI  7,1            Move to next byte in arg area
         AFI 15,-1           Decrement size of struct
         B  STRCTLP


CONT     DS 0H                End of processing curr_param
         AHI 10,8             Next parameter type
*Variadic fence: count down r12 (nfixedargs remaining)
*If r12 is already 0 (non-variadic or fence already fired), skip
*When r12 decrements to 0, force r14=4 so FLT/D use GPR path
         CFI  12,0
         BNH  VARFNC
         AHI  12,-1
         CFI  12,0
         BNE  VARFNC
         LA   14,4
VARFNC   DS   0H
*         CFI 0,3
*         BNE CONT2
*         AFI 7,7
*         N 7,=X'FFFFFFF8'

CONT2    DS   0H
         BCT 9,ARGLOOP

  
*Load function descriptor ptr (arg 1) and cif->flags return-type
*(arg 3) from caller's argument area, then read rtype->type
*for the RS handler.
CALL     DS 0H
*         SR 11,11
         LA 11,0
         LG 6,(2176+(((DSASZ+31)/32)*32))(,4)
         LGF 11,(2176+(((DSASZ+31)/32)*32)+20)(,4)

         LG 12,(2176+(((DSASZ+31)/32)*32)+8)(,4) ecif
         LG 12,0(,12) ecif->cif
         LG 12,16(,12) cif->rtype
         LA 13,0
         LH 13,10(,12)

*Call the foreign function using its address given 
*to us from an xplink compiled routine

*         L 5,16(,6)
*         L 6,20(,6)
         LMG 5,6,0(6)
         BASR 7,6
         DC    X'0700'
 
*Dispatch to return handler using cif->flags (FFI390_RET_* index)

         LA 15,RTABLE
         SLL 11,2             Return type
         L  15,0(11,15)
         BR 15

* FFI390_RET_FLOAT (2): scalar float in FPR0
RF       DS 0H
         LG 7,(2176+(((DSASZ+31)/32)*32)+24)(,4)
         STE 0,0(,7)
         B RET

* FFI390_RET_DOUBLE (3): scalar double in FPR0
RD       DS 0H
         LG 7,(2176+(((DSASZ+31)/32)*32)+24)(,4)
         STD 0,0(,7)
         B RET

* FFI390_RET_LDBLE (4): long double in FPR0 (low) + FPR2 (high)
RLD      DS 0H
         LG 7,(2176+(((DSASZ+31)/32)*32)+24)(,4)
         STD 0,0(,7)
         STD 2,8(,7)
         B RET

* FFI390_RET_INT32 (5): 32-bit int in GPR3 (sign-extended)
RI       DS 0H
         LG 7,(2176+(((DSASZ+31)/32)*32)+24)(,4)
         ST 3,0(,7)
         B RET

* FFI390_RET_INT64 (6): 64-bit int/pointer in GPR3
RLL      DS 0H
         LG 7,(2176+(((DSASZ+31)/32)*32)+24)(,4)
         
*        CFI 13,1
*        BE R32S
*        
*        CFI 13,5
*        BE R8U
*        
*        CFI 13,6
*        BE R8S
*        
*        CFI 13,7
*        BE R16U
*        
*        CFI 13,8
*        BE R16S
*
*        CFI 13,9
*        BE R32U
*        
*        CFI 13,10
*        BE R32S
*
*        CFI 13,11
*        BE R64
*
*        CFI 13,12
*        BE R64
*        
*        CFI 13,14
*        BE R64
         B R64
 
R8U      DS 0H
         STC 3,0(7)
         B RET
 
R8S      DS 0H
         STC 3,0(7)
         B RET
 
R16U     DS 0H
         STH 3,0(7)
         B RET
 
R16S     DS 0H
         STH 3,0(7)
         B RET
 
R32U     DS 0H
         ST  3,0(7)
         B RET
 
R32S     DS 0H
         ST  3,0(7)
         B RET
 
R64      DS 0H
         STG 3,0(7)
         B RET

* FFI390_RET_COMPLEX_INT (10): integer-based _Complex types.
* Compiler returns real component in GPR3, imaginary in GPR2,
* each sign/zero-extended to 64 bits.
* Dispatch on rtype->size to pick the right store width:
*   size 2  -> 2 x 1-byte (STC)
*   size 4  -> 2 x 2-byte (STH)
*   size 8  -> 2 x 4-byte (ST)
*   size 16 -> 2 x 8-byte (STG)
RCPXI    DS 0H
         LG 7,(2176+(((DSASZ+31)/32)*32)+24)(,4)    rvalue ptr
         LG 9,(2176+(((DSASZ+31)/32)*32)+8)(,4)     ecif ptr
         LG 9,0(,9)                                 ecif->cif
         LG 9,16(,9)                                cif->rtype
         LG 9,0(,9)                                 rtype->size
         CFI 9,2
         BE  RCPXI1                    size==2: _Complex char
         CFI 9,4
         BE  RCPXI2                    size==4: _Complex short
         CFI 9,16
         BE  RCPXI8                    size==16: _Complex long/longlong
*                                      size==8: _Complex int (default)
RCPXI4   DS 0H
         ST  3,0(,7)            real (4 bytes)
         ST  2,4(,7)            imag (4 bytes)
         B RET
RCPXI1   DS 0H
         STC 3,0(,7)            real (1 byte)
         STC 2,1(,7)            imag (1 byte)
         B RET
RCPXI2   DS 0H
         STH 3,0(,7)            real (2 bytes)
         STH 2,2(,7)            imag (2 bytes)
         B RET
RCPXI8   DS 0H
         STG 3,0(,7)            real (8 bytes)
         STG 2,8(,7)            imag (8 bytes)
         B RET

* FFI390_RET_VOID (0): nothing to store
RV       DS 0H
         B RET

* FFI390_RET_STRUCT (1): struct return, size-dispatched to GPR/FPR
RS       DS 0H
         LG 7,(2176+(((DSASZ+31)/32)*32)+24)(,4)
         LG 14,(2176+(((DSASZ+31)/32)*32)+8)(,4)
         LG 14,0(,14)           ecif->cif
         LG 9,16(,14)           cif->rtype
         LLGH 11,10(,9)         rtype->type
         LG 14,0(,9)            rtype->size
         CGIBH 14,24,RSP

* now we need to determine how to return the struct
* this label determines if the struct is <= 24 bytes
* if it is we return using gprs/frprs
* we then loop over rtype->elements
* to determine whether this struct is all floating point
* or if it is contains integral types
* if we have just floating point types use FPRs as appropriate
* but only if we have just the same type of float
* a struct mixing float, double and long double will use GPRs
* i have no idea why it is done this way, but it is
* also if we use more than 2 floating point types we use GPRs
* if we have at least one integral type, we use GPRs based on size
* if the struct is > 24 bytes we have already passed in the return
* pointer as the first argument to the callee "fn" 
* so that work was outsource to the compiler
* turning this into some simple cases for clarity
* if struct <= 24 bytes 
*   if elements contains >1 integral types -> GPRs
*   if elements contains only one type of float
*     if num elements <= 2 -> FPRs
*     else -> GPRs
         

* currently we have 
* r9 = cif->rtype
* r7 = ecif->rvalue
RSREG    DS 0H
         LA 15,SSTOR
         STG  1,0(,15)
         STG  2,8(,15)
         STG  3,16(,15)
         B RSCPY

* FFI390_RET_STRUCT_FF (7): struct{float;float} or _Complex float
* both floats in FPR0 (low 4 bytes) and FPR2 (low 4 bytes)
STOREF1  DS 0H
         LG 7,(2176+(((DSASZ+31)/32)*32)+24)(,4)
         STE 0,0(,7)
         STE 2,4(,7)
         B RET

* FFI390_RET_STRUCT_DD (8): struct{double;double} or _Complex double
* first double in FPR0, second double in FPR2
STORED1  DS 0H
         LG 7,(2176+(((DSASZ+31)/32)*32)+24)(,4)
         STD 0,0(,7)
         STD 2,8(,7)
         B RET

* FFI390_RET_STRUCT_LDLD (9): struct{ldbl;ldbl} or _Complex long double
* FPR0+FPR2 hold first long double, FPR4+FPR6 hold second
STORELD1 DS 0H
         LG 7,(2176+(((DSASZ+31)/32)*32)+24)(,4)
         STD 0,0(,7)
         STD 2,8(,7)
         STD 4,16(,7)
         STD 6,24(,7)
         B RET
        
* byte-by-byte copy from SSTOR (r15) into rvalue (r7), r14 bytes
RSCPY    DS 0H
         CFI 14,0
         BNH RET
         LB 1,0(,15)
         STC 1,0(,7)
         LA 15,1(,15)
         LA 7,1(,7)
         AFI 14,-1
         B RSCPY


* struct > 24 bytes: hidden first arg was the rvalue pointer,
* the callee already wrote the result there so nothing to do
RSP      DS 0H                 
         B RET                  
  
RET      DS 0H
         LD 0,SAVFPRS
         LD 2,SAVFPRS+8
         LD 4,SAVFPRS+16
         LD 6,SAVFPRS+24
         LD 8,SAVFPRS+32
         LD 9,SAVFPRS+40
         LD 10,SAVFPRS+48
         LD 11,SAVFPRS+56
         LD 12,SAVFPRS+64
         LD 13,SAVFPRS+72
         LD 14,SAVFPRS+80
         LD 15,SAVFPRS+88
         CELQEPLG

ATABLE DC A(VOID)
 DC A(I)                Labels for parm types
 DC A(FLT)  
 DC A(D)    
 DC A(LD)
 DC A(UI8)
 DC A(SI8)
 DC A(UI16)
 DC A(SI16)
 DC A(UI32)
 DC A(SI32)
 DC A(UI64)
 DC A(SI64)
* FFI_TYPE_STRUCT (13): check for HFA, else plain struct
 DC A(STRCTDISP)
 DC A(PTR)
* FFI_TYPE_COMPLEX (15): classify by elements[0]->type
 DC A(CPXANY)
 DC A(CPXF)
 DC A(CPXD)
 DC A(CPXLD)

I32 DC A(IGPR1)               Labels for passing INT in gpr
 DC A(IGPR2)
 DC A(IGPR3)
 DC A(IARGA)                  Label to store INT in arg area
U32 DC A(UGPR1)               Labels for passing u_INT in gpr
 DC A(UGPR2)
 DC A(UGPR3)
 DC A(UARGA)                  Label to store u_INT in arg area
I8 DC A(I8GPR1)               Labels for passing CHAR in gpr
 DC A(I8GPR2)
 DC A(I8GPR3)
 DC A(I8ARGA)                 Label to store CHAR in arg area
U8 DC A(U8GPR1)               Labels for passing u_CHAR in gpr
 DC A(U8GPR2)
 DC A(U8GPR3)
 DC A(U8ARGA)                 Label to store u_CHAR in arg area
U16 DC A(U16GPR1)             Labels for passing u_SHORT in gpr
 DC A(U16GPR2)
 DC A(U16GPR3)
 DC A(U16ARGA)                Label to store u_SHORT in arg area
S16 DC A(S16GPR1)             Labels for passing s_SHORT in gpr
 DC A(S16GPR2)
 DC A(S16GPR3)
 DC A(S16ARGA)                Label to store s_SHORT in arg area
S64 DC A(S64GPR1)             Labels to store INT64 in gpr
 DC A(S64GPR2)
 DC A(S64GPR3)
 DC A(S64ARGA)                Label to store INT64 in arg area
PTRS DC A(PTRG1)              Labels to store PTR in gpr
 DC A(PTRG2)
 DC A(PTRG3)
 DC A(PTRARG)                 Label to store PTR in arg area
FLD DC A(FLDR0)               Labels to store DOUBLE in fpr
 DC A(FLDR2)
 DC A(FLDR4)
 DC A(FLDR6)
 DC A(ARGAFL)                 Label to store DOUBLE in arg area
LDD DC A(DFPR0)               Labels to store l_DOUBLE in fpr
 DC A(DFPR4)
 DC A(DFPR4)
 DC A(DARGF)
 DC A(DARGF)                  Label to store l_DOUBLE in arg area
UI64S DC A(U64GP1)           Labels to store u_INT64 in gpr 
  DC A(U64GP2)
  DC A(U64GP3)
  DC A(U64ARGA)               Label to store u_INT64 in arg area
FLTS DC A(FLTR0)              Labels to store FLOAT in fpr
  DC A(FLTR2)
  DC A(FLTR4)
  DC A(FLTR6)
  DC A(ARGAFT)                Label to store FLOAT in arg area
RTABLE DC A(RV)
 DC A(RS)
 DC A(RF)
 DC A(RD)
 DC A(RLD)
 DC A(RI)
 DC A(RLL)
 DC A(STOREF1)
 DC A(STORED1)
 DC A(STORELD1)
 DC A(RCPXI)
CEEDSAHP CEEDSA SECTYPE=XPLINK
* this is the arg area for the outgoing call
ARGSL DS  XL1200
TEST DS XL8
* storage space for the returns regs maximum 4*8 fpr
SSTOR DS  XL32
SSIZE DS  XL8
SAVREGS DS XL128
SAVFPRS DS XL96
* storage space for scratch registers
SCRREGS DS XL128
* full GPR save area for STRCTDISP CELQCALL (r0-r15, 128 bytes)
SDALL  DS XL128
* single slot holding the ffi_type* passed by ref to STFPTYP
STFARG DS XL8
DSASZ    EQU (*-CEEDSAHP_FIXED)
 END FFISYS

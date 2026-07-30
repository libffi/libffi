* Copyright IBM Corp. and others 2026


* INCOMING ARGUMENTS
*   r1/r2/r3 carry the first three arguments; args 4+ live in
*   the caller's outgoing argument area at r4+2176+(rounded DSA).
*   Offsets are relative to that arg-area base:
*
*     +0   r1    closure address 
*     +8   r2    old stack pointer (used to argument area)
*     +20  r3    cif->flags    (FFI390_RET_* constant)

FFISYS2 CELQPRLG DSASIZE=DSASZ2,PSECT=FFISYS22

         USING  CEEDSAHP,4
         
* register for save buffer 
         LA 13,REGSAV

* store the incoming arguments, into the trampoline's arg area
        STG 1,(2176+(((DSASZ2+31)/32)*32))(,4)
        STG 2,(2176+(((DSASZ2+31)/32)*32)+8)(,4)
        STG 3,(2176+(((DSASZ2+31)/32)*32)+16)(,4)

* load the stored GPRs from the trampoline's caller's arg area
        LG 11,2176(,2)
        STG 11,0(,13)

        LG 11,2176+8(,2)
        STG 11,8(,13)

        LG 11,2176+16(,2)
        STG 11,16(,13)

* store old stack pointer
        STG 2,24(,13)

* make sure to save return address
        STG 7,TRAMPR

* store the floats here to minimize trampoline
         STD 0,128(,13)
         STD 1,136(,13)
         STD 2,144(,13)
         STD 3,152(,13)
         STD 4,160(,13)
         STD 5,168(,13)
         STD 6,176(,13)

         LA 2,RET
         LR 3,13

* call the function to actually figure everything out
         CELQCALL   CLSRHLP,WORKREG=10

* figure out what kind of return
* we're dealing with here
         LA 1,(2176+(((DSASZ2+31)/32)*32))(,4)
         LG 1,0(,1)
         CELQCALL   DETRET,WORKREG=10

* restore trampoline return address before we branch back through it
         LG 7,TRAMPR

         LA 1,RET

         LA 15,RTABLE
         SLL 3,2
         L 15,0(3,15)
         BR 15

RET0     DS 0H
         B RETURN
RET1     DS 0H
         LG 3,0(,1)
         B RETURN
RET2     DS 0H
         LG 1,0(,1)
         B RETURN
RET3     DS 0H
         LG 2,8(,1)
         LG 1,0(,1)
         B RETURN
RET4     DS 0H
         LG 3,16(,1)
         LG 2,8(,1)
         LG 1,0(,1)
         B RETURN
RET5     DS 0H
         LD 0,0(,1)
         B RETURN
RET6     DS 0H
         LD 0,0(,1)
         LD 2,8(,1)
         B RETURN
RET7     DS 0H
         LD 0,0(,1)
         LD 2,4(,1)
         B RETURN
RET8     DS 0H
         LD 0,0(,1)
         LD 2,8(,1)
         LD 4,16(,1)
         LD 6,24(,1)
         B RETURN

RET9     DS 0H
         LA  9,CLOSURE
         LG  9,0(,9)            closure ptr
         LG  9,128(,9)          closure->cif
         LG  9,16(,9)           cif->rtype
         LG  9,0(,9)            rtype->size (total complex size)
         CFI 9,2
         BE  RET9A              size==2: _Complex char (1+1)
         CFI 9,4
         BE  RET9B              size==4: _Complex short (2+2)
         CFI 9,16
         BE  RET9D              size==16: _Complex long/longlong (8+8)
*                               size==8: _Complex int (4+4) default
         LGF 3,0(,1)            real (4 bytes, sign-extend)
         LGF 2,4(,1)            imag (4 bytes, sign-extend)
         B RETURN
RET9A    DS 0H
         LGB 3,0(,1)            real (1 byte, sign-extend)
         LGB 2,1(,1)            imag (1 byte, sign-extend)
         B RETURN
RET9B    DS 0H
         LGH 3,0(,1)            real (2 bytes, sign-extend)
         LGH 2,2(,1)            imag (2 bytes, sign-extend)
         B RETURN
RET9D    DS 0H
         LG  3,0(,1)            real (8 bytes)
         LG  2,8(,1)            imag (8 bytes)
         B RETURN

         LA 3,RET
         LG 3,0(,3)

RETURN   DS 0H
         CELQEPLG

RTABLE  DC A(RET0)
 DC A(RET1)
 DC A(RET2)
 DC A(RET3)
 DC A(RET4)
 DC A(RET5)
 DC A(RET6)
 DC A(RET7)
 DC A(RET8)
 DC A(RET9)

CEEDSAHP CEEDSA SECTYPE=XPLINK
ARGAREA DS XL32
CLOSURE DS   XL8
CIF  DS   XL8
RET  DS   XL32
RETP DS   XL8
USER DS   XL8
TRAMPR DS  XL8

* save area for registers at time of trampoline entry
REGSAV DS   XL192

DSASZ2   EQU (*-CEEDSAHP_FIXED)
 END FFISYS2

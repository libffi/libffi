* Copyright IBM Corp. and others 2026


*   Incoming register layout 
*   r0        closure ptr (not saved/preserved; trampoline use only)
*   r1/r2/r3  caller's first three arguments (untouched by trampoline;
*             saved by CELQPRLG at 2176(,r4_caller))
*   r4        caller's stack pointer (CELQPRLG adjusts before body;
*             r4_caller recovered from backchain at r4_new+2048)
*   r5,r6     standard env/entry point registers of this function
*   r7-r15    untouched by trampoline, registers from callee       

FFISYS2 CELQPRLG DSASIZE=DSASZ2,PSECT=FFISYS22

         USING  CEEDSAHP,4
         
* register for save buffer 
* this goes into the helper
        LA 13,REGSAV

* recover caller's r4 from the backchain in the save area CELQPRLG
        LG 2,2048(,4)

* load original r1/r2/r3 that CELQPRLG saved at 2176(,r4_caller)
        LG 11,2176(,2)
        STG 11,0(,13)

        LG 11,2176+8(,2)
        STG 11,8(,13)

        LG 11,2176+16(,2)
        STG 11,16(,13)

* store caller's stack pointer (becomes reg->gpr[3] for helper)
        STG 2,24(,13)

* save closure ptr from r0 into DSA slot so we can restore it later
        STG 0,CLOSURE

* store the floats here to minimize trampoline
* these are used in the helper if there are float arguments 
         STD 0,128(,13)
         STD 1,136(,13)
         STD 2,144(,13)
         STD 3,152(,13)
         STD 4,160(,13)
         STD 5,168(,13)
         STD 6,176(,13)

* r1=closure  r2=retbuf  r3=reg (addr of REGSAV struct)
         LG  1,CLOSURE
         LA  2,RET
         LGR 3,13
         CELQCALL   CLSRHLP,WORKREG=10

* reload closure ptr (CELQCALL clobbered r1) then determine return type
         LG  1,CLOSURE
         CELQCALL   DETRET,WORKREG=10


         LA 1,RET

* load table of return labels
* shift return value from DETRET to use as offset into table
* then just branch to label for that return handling
         LA 15,RTABLE
         SLL 3,2
         L 15,0(3,15)
         BR 15

* hidden pointer (struct > 24 bytes), callee already wrote through it
* so nothing to load
RET0     DS 0H
         B RETURN
* void, int, or pointer, result in GPR3
RET1     DS 0H
         LG 3,0(,1)
         B RETURN
* small struct <=8 bytes, fits in GPR1
RET2     DS 0H
         LG 1,0(,1)
         B RETURN
* medium struct 9-16 bytes, GPR1 and GPR2
RET3     DS 0H
         LG 2,8(,1)
         LG 1,0(,1)
         B RETURN
* large struct 17-24 bytes, GPR1 + GPR2 + GPR3
RET4     DS 0H
         LG 3,16(,1)
         LG 2,8(,1)
         LG 1,0(,1)
         B RETURN
* scalar float or double, result in FPR0
RET5     DS 0H
         LD 0,0(,1)
         B RETURN
* long double, struct{double;double}, or _Complex double
* FPR0 + FPR2 at 8-byte stride
RET6     DS 0H
         LD 0,0(,1)
         LD 2,8(,1)
         B RETURN
* struct{float;float} or _Complex float, FPR0 + FPR2 at 4-byte stride
RET7     DS 0H
         LD 0,0(,1)
         LD 2,4(,1)
         B RETURN
* struct{ldbl;ldbl} or _Complex long double, FPR0 + FPR2 + FPR4 + FPR6
RET8     DS 0H
         LD 0,0(,1)
         LD 2,8(,1)
         LD 4,16(,1)
         LD 6,24(,1)
         B RETURN
* _Complex int/short/char/long: real in GPR3, imag in GPR2, sized
RET9     DS 0H
         LA  9,CLOSURE
         LG  9,0(,9)            closure ptr
         LG  9,128(,9)          closure->cif
         LG  9,16(,9)           cif->rtype
         LG  9,0(,9)            rtype->size (total complex size)
         CFI 9,2
         BE  RET9A              size=2  _Complex char (1+1)
         CFI 9,4
         BE  RET9B              size=4  _Complex short (2+2)
         CFI 9,16
         BE  RET9D              size=16 _Complex long/longlong (8+8)
*                               size=8  _Complex int (4+4) default
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
RET  DS   XL32

* save area for registers at time of trampoline entry
REGSAV DS   XL192

DSASZ2   EQU (*-CEEDSAHP_FIXED)
 END FFISYS2

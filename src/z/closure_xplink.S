* Copyright IBM Corp. and others 2026
FFISYS2 CELQPRLG DSASIZE=DSASZ2,PSECT=FFISYS22

         USING  CEEDSAHP,4
         
* store the floats here to minimize trampoline
         STD 0,216(,1)
         STD 1,224(,1)
         STD 2,232(,1)
         STD 3,240(,1)
         STD 4,248(,1)
         STD 5,256(,1)
         STD 6,264(,1)

         LA 2,CLOSURE
         STG 1,0(,2)

         LA 2,RET

* call the function to actually figure everything out
         CELQCALL   CLSRHLP,WORKREG=10

* figure out what kind of return 
* we're dealing with here
         LA 1,CLOSURE
         LG 1,0(,1)
         CELQCALL   DETRET,WORKREG=10

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

* RET9: integer-based _Complex — real at ret[0], imag at ret[element_size].
* Caller expects real in GPR3, imaginary in GPR2, sign-extended to 64 bits.
* Dispatch on rtype->size to pick load width.
RET9     DS 0H
         LA  9,CLOSURE
         LG  9,0(,9)            closure ptr
         LG  9,64(,9)           closure->cif
         LG  9,16(,9)           cif->rtype
         LG  9,0(,9)            rtype->size (total complex size)
         CFI 9,2
         BE  RET9A              size==2: _Complex char (1+1)
         CFI 9,4
         BE  RET9B              size==4: _Complex short (2+2)
         CFI 9,16
         BE  RET9D              size==16: _Complex long/longlong (8+8)
*                               size==8: _Complex int (4+4) — default
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
CLOSURE DS   XL8
CIF  DS   XL8
RET  DS   XL32
RETP DS   XL8
USER DS   XL8

REG1 DS   XL8

DSASZ2   EQU (*-CEEDSAHP_FIXED)
 END FFISYS2

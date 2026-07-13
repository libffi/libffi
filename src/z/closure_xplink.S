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

CEEDSAHP CEEDSA SECTYPE=XPLINK
CLOSURE DS   XL8
CIF  DS   XL8
RET  DS   XL16
RETP DS   XL8
USER DS   XL8

REG1 DS   XL8

DSASZ2   EQU (*-CEEDSAHP_FIXED)
 END FFISYS2

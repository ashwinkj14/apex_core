MOVC R2,#2
MOVC R3,#1
MOVC R1,#0
MOVC R7,#0
MOVC R0,#2
AND R4,R7,R3
CMP R6,R1,R4
JUMP R1,#16
MUL R5,R0,R2
STORE R3,R1,#4
CMP R6,R1,R4
BZ #-20
LOAD R5,R1,#4
ADDL R7,R7,#1
CMP R6,R7,R0
BNZ #-40
HALT
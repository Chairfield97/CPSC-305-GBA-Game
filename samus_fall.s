@samus_fall.s

/* a function to calculate the offset based on the tile width*/
/* frame = samus_fall(isFalling)*/
    
.global samus_fall
samus_fall:
	cmp r0,#1 /* if r0 != 1, goto else*/
	bne .else
	mov r0,#48 /* frame = 48*/
	b .finish	
.else:
	mov r0, #0 /* frame = 0*/
.finish:
	mov pc,lr
	

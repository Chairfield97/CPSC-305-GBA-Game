@calc_offset.s

/* a function to calculate the offset based on the tile width*/
/* offset = calc_offset(offset, tileWidth)*/
 /* if width is also 64 add 0x800, else just 0x400 */
        
.global calc_offset
calc_offset:

	cmp r1,#64 /* if r1 != 64, goto else*/
	bne .else
	add r0,r0,#2048 /* offset += 0x800(2048 in decimal)*/
	b .finish	
.else:
	add r0, r0,#1024 /*offset+= 0x400(1024 in decimal)*/
.finish:
	mov pc,lr
	

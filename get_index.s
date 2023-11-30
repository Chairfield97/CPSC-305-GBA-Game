@get_index.s

/* a function to get the index for inserting text in a tile map*/
/* index = get_index(row, col) */

.global get_index
get_index:
    mov r3,#32
    mul r3,r0,r3 /* r3 = row * 32 */
	add r3,r3,r1 /* r3 += col*/
	mov r0,r3
.finish:
	mov pc,lr
	

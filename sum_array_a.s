.global sum_array_a
.func sum_array_a
//r0 -> *array, r1 -> len, r2 -> i, r3 -> sum 
sum_array_a:
	mov r2, #0
	mov r3, #0
loop:
	cmp r2, r1
	beq endif
	ldr r12, [r0]
	add r3, r3, r12
	add r0, r0, #4
	add r2, r2, #1
	b loop
endif:
	mov r0, r3
	bx lr
	

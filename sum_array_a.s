	.global sum_array_a
	.func sum_array_a

	/* r0 - int *array */
	/* r1 - int n */
	/* r2 - int i */
	/* r3 - int sum */
	/* r4 - array[i] */

sum_array_a:
	mov r3, #0
	mov r2, #1
	cmp r1, #0
	beq endloop
	ldr r4, [r0]
	add r3, r3, r4
	add r0, r0, #4
loop:
	cmp r2, r1
	beq endloop
	ldr r4, [r0]
	add r3, r3, r4
	add r2, r2, #1
	add r0, r0, #4
	b loop
endloop:
	mov r0, r3
	bx lr

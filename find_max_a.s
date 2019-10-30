	.global find_max_a
	.func find_max_a

	/* r0 - int *array */
	/* r1 - int n */
	/* r2 - int i */
	/* r3 - int max */
	/* r4 - int array[i] */
find_max_a:
	mov r2, #1
	mov r3, #0
	cmp r1, #0
	beq endloop
	ldr r3, [r0]
	add r0, r0, #4
loop:
	cmp r2, r1
	beq endloop
	ldr r4, [r0]
	cmp r4, r3
	bgt change_max
continue:
	add r2, r2, #1
	add r0, r0, #4
	b loop
change_max:
	mov r3, r4
	b continue
endloop:
	mov r0, r3
	bx lr

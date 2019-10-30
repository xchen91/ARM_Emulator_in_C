	.global fib_iter_a
	.func fib_iter_a

	/* r0 - int n */
	/* r1 - int prev_prev_num */
	/* r2 - int prev_num */
	/* r3 - int cur_num */
	/* r4 - int i */
fib_iter_a:
	mov r1, #0
	mov r2, #0
	mov r3, #1
	cmp r0, #0
	beq n_is_0
	mov r4, #1
loop:
	cmp r4, r0
	beq endloop
	mov r1, r2
	mov r2, r3
	add r3, r1, r2
	add r4, r4, #1
	b loop
endloop:
	mov r0, r3
	bx lr
n_is_0:
	mov r0, #0
	bx lr

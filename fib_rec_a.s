	.global fib_rec_a
	.func fib_rec_a

	/* r0 - int n */
	/* r2 - prev_num */
fib_rec_a:
	sub sp, sp, #24
	str lr, [sp]
	str r2, [sp, #8]
	cmp r0, #1
	bgt rec
	b end_rec
rec:
	str r0, [sp, #4]
	sub r0, r0, #1
	bl fib_rec_a
	mov r2, r0
	ldr r0, [sp, #4]
	sub r0, r0, #2
	bl fib_rec_a
	add r0, r0, r2
	
end_rec:
	ldr r2, [sp, #8]
	ldr lr, [sp]
	add sp, sp, #24
	bx lr

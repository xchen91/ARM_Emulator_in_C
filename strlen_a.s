	.global strlen_a
	.func strlen_a

	/* r0 - char *s */
	
strlen_a:
	mov r1, #0
loop:
	ldrb r12, [r0]
	cmp r12, #0
	beq endloop
	add r0, r0, #1
	add r1, r1, #1
	b loop
endloop:
	mov r0, r1
	bx lr

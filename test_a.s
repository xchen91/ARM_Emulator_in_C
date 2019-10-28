	.global test_a
	.func test_a

test_a:
	mul r2, r0, r2
	add r2, r2, r3
	mul r12, r0, r0
	mul r0, r12, r1
	add r0, r0, r2
       // mov r0, r2
	bx lr

	.global test_b_a
	.func test_b_a

test_b_a:
	cmp r0, r1
	bgt done
	add r0, r0, #10	
	bx lr

done:
	add r0, r0, #10
	bx lr

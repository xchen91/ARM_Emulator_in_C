	.global quadratic_a
	.func quadratic_a

	/* r0 - int x */
	/* r1 - int a */
	/* r2 - int b */
	/* r3 - int c */
	/* r4 - x * x */
	/* r5 - r4 * r1 */
	/* r6 - r2 * r0 */
	/* r7 - r5 + r6 */
	/* r8 - r7 + r3 */
quadratic_a:
	mul r4, r0, r0
	mul r5, r1, r4
	mul r6, r0, r2
	add r7, r5, r6 
	add r8, r7, r3
	mov r0, r8
	bx lr

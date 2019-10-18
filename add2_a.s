    .global add2_a
    .func add2_a

add2_a:
    add r0, r0, r1
    add r0, r0, r0
    bx lr

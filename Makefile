PROGS = armemu

OBJS_ARMEMU = sum_array_a.o test_b_a.o fib_rec_a.o

CFLAGS = -g

%.o : %.s
	as -o $@ $<

%.o : %.c
	gcc -c ${CFLAGS} -o $@ $<

all : ${PROGS}

analyze : analyze.c ${OBJS_ANALYZE}
	gcc ${CFLAGS} -o $@ $^

armemu : armemu.c ${OBJS_ARMEMU}
	gcc ${CFLAGS} -o $@ $^

clean :
	rm -rf ${PROGS} ${OBJS_ANALYZE} ${OBJS_ARMEMU}

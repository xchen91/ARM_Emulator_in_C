PROGS = analyze armemu

OBJS_ANALYZE = addsub_a.o
OBJS_ARMEMU = add_a.o add2_a.o

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

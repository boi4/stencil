.PHONY: all

LDFLAGS=-lm

all: out/original out/debug out/o1 out/o2 out/o3

out/original: stencil.c
	gcc -std=gnu99 -Wall $^ -o $@ ${LDFLAGS}

out/debug: stencil.c
	gcc -std=gnu99 -Wall -ggdb $^ -o $@ ${LDFLAGS}

out/o1: stencil.c
	gcc -std=gnu99 -Wall -O1 $^ -o $@ ${LDFLAGS}

out/o2: stencil.c
	gcc -std=gnu99 -Wall -O2 $^ -o $@ ${LDFLAGS}

out/o3: stencil.c
	gcc -std=gnu99 -Wall -O3 $^ -o $@ ${LDFLAGS}

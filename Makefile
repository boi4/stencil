.PHONY: all,verbose

LDFLAGS=-lm

all: out/original out/debug out/o1 out/o2 out/o3 out/o3debug out/ofastdebug

out/original: stencil.c stencil_four.c
	gcc -std=gnu99 -Wall $^ -o $@ ${LDFLAGS}

out/debug: stencil.c stencil_four.c
	gcc -std=gnu99 -Wall -ggdb3 $^ -o $@ ${LDFLAGS}

out/o1: stencil.c stencil_four.c
	gcc -std=gnu99 -Wall -O1 $^ -o $@ ${LDFLAGS}

out/o2: stencil.c stencil_four.c
	gcc -std=gnu99 -Wall -O2 $^ -o $@ ${LDFLAGS}

out/o3: stencil.c stencil_four.c
	gcc -std=gnu99 -Wall -O3 $^ -o $@ ${LDFLAGS}

out/o3debug: stencil.c stencil_four.c
	gcc -std=gnu99 -Wall -O3 -ggdb3 $^ -o $@ ${LDFLAGS}

out/ofastdebug: stencil.c stencil_four.c
	gcc -std=gnu99 -Wall -Ofast -mtune=native -ggdb3 $^ -o $@ ${LDFLAGS}

verbose: stencil.c stencil_four.c
	gcc -std=gnu99 -Wall -O3 -fopt-info-all -ggdb3 $^ -o /dev/null ${LDFLAGS}

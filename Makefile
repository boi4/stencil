.PHONY: all,verbose

LDFLAGS=-lm

all: out/original out/debug out/o3 out/o3debug out/ofastdebug

out/original: out/stencil.o out/stencil_precompute.o
	gcc -std=gnu99 -Wall $^ -o $@ ${LDFLAGS}

out/debug: out/stencil.o out/stencil_precompute.o
	gcc -std=gnu99 -Wall -ggdb3 $^ -o $@ ${LDFLAGS}

out/o1: out/stencil.o out/stencil_precompute.o
	gcc -std=gnu99 -Wall -O1 $^ -o $@ ${LDFLAGS}

out/o2: out/stencil.o out/stencil_precompute.o
	gcc -std=gnu99 -Wall -O2 $^ -o $@ ${LDFLAGS}

out/o3: out/stencil.o out/stencil_precompute.o
	gcc -std=gnu99 -Wall -O3 $^ -o $@ ${LDFLAGS}

out/o3debug: out/stencil.o out/stencil_precompute.o
	gcc -std=gnu99 -Wall -O3 -ggdb3 $^ -o $@ ${LDFLAGS}

#out/ofastdebug: out/stencil.o stencil_precompute2.c
out/ofastdebug: out/stencil.o out/stencil_precompute.o
	gcc -std=gnu99 -Wall -Ofast -mtune=native -ggdb3 $^ -o $@ ${LDFLAGS}

verbose: stencil.c stencil_precompute.c
	gcc -std=gnu99 -Wall -O3 -fopt-info-all -ggdb3 $^ -o /dev/null ${LDFLAGS}

out/stencil.o: stencil.c
	gcc -std=gnu99 -c -Wall -Ofast -mtune=native -ggdb3 $^ -o $@ ${LDFLAGS}

out/stencil_precompute.o: stencil_precompute.c
	gcc -std=gnu99 -c -Wall -Ofast -mtune=native -ggdb3 $^ -o $@ ${LDFLAGS}


.PHONY: all,verbose

LDFLAGS=

all: out/original out/o1 out/o2 out/o3 out/ofast out/clangoriginal out/clango1 out/clango2 out/clango3 out/clangofast out/icco3 out/iccofast

gcc: out/original out/o1 out/o2 out/o3 out/ofast 

clang: out/clangoriginal out/clango1 out/clango2 out/clango3 out/clangofast

icc: out/icco3 out/iccofast


out/original: stencil.c stencil_precompute.c
	gcc -std=gnu99 -Wall $^ -o $@ ${LDFLAGS}

out/debug: stencil.c stencil_precompute.c
	gcc -std=gnu99 -Wall -ggdb3 $^ -o $@ ${LDFLAGS}

out/o1: stencil.c stencil_precompute.c
	gcc -std=gnu99 -Wall -O1 $^ -o $@ ${LDFLAGS}

out/o2: stencil.c stencil_precompute.c
	gcc -std=gnu99 -Wall -O2 $^ -o $@ ${LDFLAGS}

out/o3: stencil.c stencil_precompute.c
	gcc -std=gnu99 -Wall -O3 $^ -o $@ ${LDFLAGS}

out/o3debug: stencil.c stencil_precompute.c
	gcc -std=gnu99 -Wall -O3 -ggdb3 $^ -o $@ ${LDFLAGS}

out/ofast: stencil.c stencil_precompute.c
	gcc -std=gnu99 -Wall -Ofast $^ -o $@ ${LDFLAGS}

out/ofastdebug: stencil.c stencil_precompute.c
	gcc -std=gnu99 -Wall -Ofast -mtune=native -ggdb3 $^ -o $@ ${LDFLAGS}

out/ofastgp: out/stencilgp.c out/stencil_precomputegp.c
	gcc -std=gnu99 -pg -Wall -Ofast -mtune=native -ggdb3 $^ -o $@ ${LDFLAGS}



#verbose: stencil.c stencil_precompute.c
#	gcc -std=gnu99 -Wall -O3 -fopt-info-all -ggdb3 $^ -o /dev/null ${LDFLAGS}
#
#verbosestencil: stencil.c
#	gcc -std=gnu99 -c -Wall -O3 -fopt-info-all -ggdb3 $^ -o /dev/null ${LDFLAGS}




#out/stencil.o: stencil.c
#	gcc -std=gnu99 -c -Wall -Ofast -mtune=native -ggdb3 $^ -o $@ ${LDFLAGS}
#
#out/stencil_precompute.o: stencil_precompute.c
#	gcc -std=gnu99 -c -Wall -Ofast -mtune=native -ggdb3 $^ -o $@ ${LDFLAGS}
#
#out/stencilgp.o: stencil.c
#	gcc -std=gnu99 -pg -c -Wall -Ofast -mtune=native -ggdb3 $^ -o $@ ${LDFLAGS}
#
#out/stencil_precomputegp.o: stencil_precompute.c
#	gcc -std=gnu99 -pg -c -Wall -Ofast -mtune=native -ggdb3 $^ -o $@ ${LDFLAGS}




# CLANG
out/clangoriginal: stencil.c stencil_precompute.c
	clang -std=gnu99 -Wall $^ -o $@ ${LDFLAGS}

out/clango1: stencil.c stencil_precompute.c
	clang -std=gnu99 -Wall -O1 $^ -o $@ ${LDFLAGS}

out/clango2: stencil.c stencil_precompute.c
	clang -std=gnu99 -Wall -O2 $^ -o $@ ${LDFLAGS}

out/clango3: stencil.c stencil_precompute.c
	clang -std=gnu99 -Wall -O3 $^ -o $@ ${LDFLAGS}
out/clangofast: stencil.c stencil_precompute.c
	clang -std=gnu99 -Wall -Ofast $^ -o $@ ${LDFLAGS}


# ICC
out/icco3: stencil.c stencil_precompute.c
	icc -std=gnu99 -Wall -O3 $^ -o $@ ${LDFLAGS}
out/iccofast: stencil.c stencil_precompute.c
	icc -std=gnu99 -Wall -Ofast $^ -o $@ ${LDFLAGS}

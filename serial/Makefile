.PHONY: all,verbose

LDFLAGS=
CFLAGS=-mtune=native -march=broadwell



all: stencil

stencil: stencil.c stencil_precompute.c
	icc -simd ${CFLAGS} -Ofast $^ -o $@ ${LDFLAGS}






#all: out/original out/o1 out/o2 out/o3 out/ofast out/clangoriginal out/clango1 out/clango2 out/clango3 out/clangofast out/icco3 out/iccofast

gcc: out/original out/o1 out/o2 out/o3 out/ofast 

clang: out/clangoriginal out/clango1 out/clango2 out/clango3 out/clangofast

icc: out/icco3 out/iccofast out/iccadv


out/original: stencil.c stencil_precompute.c
	gcc -std=gnu99 -Wall ${CFLAGS} $^ -o $@ ${LDFLAGS}

out/debug: stencil.c stencil_precompute.c
	gcc -std=gnu99 -Wall ${CFLAGS} -ggdb3 $^ -o $@ ${LDFLAGS}

out/o1: stencil.c stencil_precompute.c
	gcc -std=gnu99 -Wall ${CFLAGS} -O1 $^ -o $@ ${LDFLAGS}

out/o2: stencil.c stencil_precompute.c
	gcc -std=gnu99 -Wall ${CFLAGS} -O2 $^ -o $@ ${LDFLAGS}

out/o3: stencil.c stencil_precompute.c
	gcc -std=gnu99 -Wall ${CFLAGS} -O3 $^ -o $@ ${LDFLAGS}

out/o3debug: stencil.c stencil_precompute.c
	gcc -std=gnu99 -Wall ${CFLAGS} -O3 -ggdb3 $^ -o $@ ${LDFLAGS}

out/ofast: stencil.c stencil_precompute.c
	gcc -std=gnu99 -Wall ${CFLAGS} -Ofast $^ -o $@ ${LDFLAGS}

out/ofastdebug: stencil.c stencil_precompute.c
	gcc -std=gnu99 -Wall ${CFLAGS} -Ofast -mtune=native -ggdb3 $^ -o $@ ${LDFLAGS}

out/ofastgp: out/stencilgp.c out/stencil_precomputegp.c
	gcc -std=gnu99 -pg -Wall ${CFLAGS} -Ofast -mtune=native -ggdb3 $^ -o $@ ${LDFLAGS}



#verbose: stencil.c stencil_precompute.c
#	gcc -std=gnu99 -Wall ${CFLAGS} -O3 -fopt-info-all -ggdb3 $^ -o /dev/null ${LDFLAGS}
#
#verbosestencil: stencil.c
#	gcc -std=gnu99 -c -Wall ${CFLAGS} -O3 -fopt-info-all -ggdb3 $^ -o /dev/null ${LDFLAGS}




#out/stencil.o: stencil.c
#	gcc -std=gnu99 -c -Wall ${CFLAGS} -Ofast -mtune=native -ggdb3 $^ -o $@ ${LDFLAGS}
#
#out/stencil_precompute.o: stencil_precompute.c
#	gcc -std=gnu99 -c -Wall ${CFLAGS} -Ofast -mtune=native -ggdb3 $^ -o $@ ${LDFLAGS}
#
#out/stencilgp.o: stencil.c
#	gcc -std=gnu99 -pg -c -Wall ${CFLAGS} -Ofast -mtune=native -ggdb3 $^ -o $@ ${LDFLAGS}
#
#out/stencil_precomputegp.o: stencil_precompute.c
#	gcc -std=gnu99 -pg -c -Wall ${CFLAGS} -Ofast -mtune=native -ggdb3 $^ -o $@ ${LDFLAGS}




# CLANG
out/clangoriginal: stencil.c stencil_precompute.c
	clang -std=gnu99 -Wall ${CFLAGS} $^ -o $@ ${LDFLAGS}

out/clango1: stencil.c stencil_precompute.c
	clang -std=gnu99 -Wall ${CFLAGS} -O1 $^ -o $@ ${LDFLAGS}

out/clango2: stencil.c stencil_precompute.c
	clang -std=gnu99 -Wall ${CFLAGS} -O2 $^ -o $@ ${LDFLAGS}

out/clango3: stencil.c stencil_precompute.c
	clang -std=gnu99 -Wall ${CFLAGS} -O3 $^ -o $@ ${LDFLAGS}
out/clangofast: stencil.c stencil_precompute.c
	clang -std=gnu99 -Wall ${CFLAGS} -Ofast $^ -o $@ ${LDFLAGS}


# ICC
out/icco3: stencil.c stencil_precompute.c
	icc -std=gnu99 -Wall ${CFLAGS} -O3 $^ -o $@ ${LDFLAGS}
out/iccofast: stencil.c stencil_precompute.c
	icc -simd ${CFLAGS} -Ofast $^ -o $@ ${LDFLAGS}
out/iccadv: stencil.c stencil_precompute.c
	icc -std=gnu99 -g -simd -qopt-report=5 -Wall ${CFLAGS} -Ofast $^ -o $@ ${LDFLAGS}

out/icclaptop: stencil.c stencil_precompute.c
	icc -std=gnu99 -g -xCORE-AVX2 -simd -qopt-report=5 -Wall -Ofast $^ -o $@ ${LDFLAGS}

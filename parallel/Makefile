.PHONY: all,submit

LDFLAGS=-lm
CFLAGS=-mtune=native -march=broadwell


all: out/mpistencil

submit: submission.zip


# =========================== MPI =======================================
out/mpistencil: stencil.c util.c stencil.h
	#mpiicc ${CFLAGS} -g -simd -Ofast $^ -o $@ ${LDFLAGS}
	mpiicc ${CFLAGS} -g -simd -Ofast stencil.c -o $@ ${LDFLAGS}


# ========================== SERIAL ====================================
out/stencilserial: stencil.c stencil_precompute.c
	icc -simd ${CFLAGS} -Ofast $^ -o $@ ${LDFLAGS}


out/iccadv: stencil.c stencil_precompute.c
	icc -std=gnu99 -g -simd -qopt-report=5 -Wall ${CFLAGS} -Ofast $^ -o $@ ${LDFLAGS}


submission.zip: report.pdf env.sh Makefile stencil.c
	zip $@ $^

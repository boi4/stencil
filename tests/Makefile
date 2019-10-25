all: stencil_original stencil_row stencil_vectorized stencil_float stencil_inplace stencil_aligned

stencil_original: stencil_original.c
	gcc -g -o stencil_original stencil_original.c

stencil_row: stencil_row.c
	gcc -g -o stencil_row stencil_row.c

stencil_vectorized: stencil_vectorized.c
	icc -g -xCORE-AVX2 -simd -o stencil_vectorized stencil_vectorized.c

stencil_float: stencil_float.c
	icc -g -xCORE-AVX2 -simd -o stencil_float stencil_float.c

stencil_inplace: stencil_inplace.c
	icc -g -xCORE-AVX2 -simd -o stencil_inplace stencil_inplace.c

stencil_aligned: stencil_aligned.c
	icc -g -xCORE-AVX2 -simd -o stencil_aligned stencil_aligned.c

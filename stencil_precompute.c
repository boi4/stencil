#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/mman.h>

#include "stencil.h"

extern void output_image(const char* file_name, const int nx, const int ny,
                  const int width, const int height, float* image);


// debug functions defined at the end
void print_row(float *row, size_t length);
void dump_field(char *fname, int nx, int ny, float *field);


/* This function will stencil the part where four tiles meet
 * it will simulate an infinite space by wrapping around the corners
 * probably could be optimized by using the symmetry following the diagonals (using only one fourth of small_field)
 * TODO: optimize
 */
float *precompute_center(size_t niters) {
  static float center_small[64][64]; // [row][column], compiler will make two elements on same row be next to each other
  static float center_big[128][128]; // will be computed out of center_small

  static float row_buffer[64]; // holds the previous row
  static float row_buffer2[64]; // holds the previous row
  static float row_buffer3[64]; // holds the previous row

  float tmp;
  float * restrict prev_row_bak, * restrict cur_row_bak, * restrict tmp2;

  // create chessboard pattern
  for (size_t i = 0; i < 32; i++) {
    row_buffer[i] = WHITE_FLOAT;
  }
  for (size_t i = 32; i < 64; i++) {
    row_buffer[i] = 0.0f;
  }
  for (size_t i = 32; i < 64; i++) {
      memcpy(center_small[i], row_buffer, sizeof(row_buffer));
  }
  for (size_t i = 0; i < 32; i++) {
    row_buffer[i] = 0.0f;
  }
  for (size_t i = 32; i < 64; i++) {
    row_buffer[i] = WHITE_FLOAT;
  }
  for (size_t i = 0; i < 32; i++) {
      memcpy(center_small[i], row_buffer, sizeof(row_buffer));
  }
  // we first do the lower half because we don't have to init row_buffer again

  prev_row_bak = row_buffer;
  cur_row_bak = row_buffer2;

  for (size_t i = 0; i < niters; i++) {
    for (size_t row = 0; row < 63; row++) {
      float * restrict cur_row = center_small[row];
      row_buffer3[ 0] = center_small[63-row][63] + cur_row[ 0] * 6.0f + cur_row[ 1]; // TODO: first value will be wrong
      for (size_t col = 1; col < 63; col++) {
        row_buffer3[col] = cur_row[col] * 6.0f + cur_row[col+1] + cur_row[col-1];
      }
      row_buffer3[63] = center_small[63-row][0] + cur_row[63]* 6.0f + cur_row[62]; // TODO: first value will be wrong

      memcpy(cur_row_bak, cur_row, 64 * sizeof(float));

      // row additions
      // Add previous and following line
      float * restrict nxt_row = cur_row + 64;
      for (size_t col = 0; col < 64; col++) {
        tmp = prev_row_bak[col] + nxt_row[col] + row_buffer3[col];
        center_small[row][col] = tmp * 0.1f;
      }

      // switch buffers
      tmp2 = prev_row_bak;
      prev_row_bak = cur_row_bak;
      cur_row_bak = tmp2;
    }

    // special case last row
    float * restrict cur_row = center_small[63];
    row_buffer3[ 0] = center_small[0][63] + cur_row[ 0] * 6.0f + cur_row[ 1]; // TODO: first value will be wrong
    for (size_t col = 1; col < 63; col++) {
      row_buffer3[col] = cur_row[col] * 6.0f + cur_row[col+1] + cur_row[col-1];
    }
    row_buffer3[63] = center_small[0][0] + cur_row[63]* 6.0f + cur_row[62]; // TODO: first value will be wrong

    // TODO: vectorize
    // Add previous and following line
    float * restrict nxt_row = center_small[0];
    for (size_t col = 0; col < 64; col++) {
      tmp = prev_row_bak[col] + nxt_row[63-col] + row_buffer3[col];
      center_small[63][col] = tmp * 0.1f;
    }

    // set reverse row for the first line in next iteration
    for(size_t i = 0; i < 64; i++) {cur_row_bak[i] = center_small[63][63-i];}

    // switch buffers
    tmp2 = prev_row_bak;
    prev_row_bak = cur_row_bak;
    cur_row_bak = tmp2;
  }


  // fill big field
  // copy small field into the middle
  for (size_t row = 32; row < 96; row++) {
    for (size_t col = 32; col < 96; col++) {
      center_big[row][col] = center_small[row-32][col-32];
    }
  }

  // mirror small field on the left
  for (size_t row = 32; row < 96; row++) {
    for (size_t col = 0; col < 32; col++) {
      center_big[row][col] = center_big[row][63-col];
    }
  }

  // mirror small field on the right
  for (size_t row = 32; row < 96; row++) {
    for (size_t col = 96; col < 128; col++) {
      center_big[row][col] = center_big[127-row][col-64];
    }
  }

  // mirror whole thing on top
  for (size_t row = 0; row < 32; row++) {
    for (size_t col = 0; col < 128; col++) {
      center_big[row][col] = center_big[63-row][col];
    }
  }

  // mirror whole thing on bottom
  for (size_t row = 96; row < 128; row++) {
    for (size_t col = 0; col < 128; col++) {
      center_big[row][col] = center_big[row-64][127-col];
    }
  }

  //dump_field("test.pgm", 64, 64, center_small[0]);
  return *center_big;
}




void stencil_border_part(size_t border_size,
                         float * const restrict vert_field_arg,
                         bool reverse // wether to go from the bottom to the top
                         ) {
  float * restrict vert_field = __builtin_assume_aligned(vert_field_arg, BORDER_FIELD_ALIGNMENT);
  float tmp;

  static float row_buffer1[64] __attribute__ ((aligned (0x100)));
  static float row_buffer2[64] __attribute__ ((aligned (0x100)));
  static float row_buffer3[64] __attribute__ ((aligned (0x100)));

  float * restrict prev_row_bak, * restrict cur_row_bak, * restrict tmp2;

  prev_row_bak = row_buffer1; // holds the original previous row
  cur_row_bak = row_buffer2;  // holds the original current row

  // start stencilin'
  for (size_t num_rows = 2 * border_size - 1; num_rows >= border_size; num_rows--) {

    memset(prev_row_bak, 0, 64 * sizeof(float)); // first 'invisible' row is black

    for (size_t row = 0; row < num_rows; row++) {
      float * restrict cur_row = vert_field + (reverse ? (2 * border_size - row) << 6: row << 6);

      // stencil over the horizontal
      row_buffer3[ 0] = cur_row[ 0] * 7.0f + cur_row[ 1]; // we are mirroring on the left
      for (size_t col = 1; col < 63; col++) {
        row_buffer3[col] = cur_row[col] * 6.0f + cur_row[col+1] + cur_row[col-1];
      }
      row_buffer3[63] = cur_row[63] * 7.0f + cur_row[62]; // we are mirroring on the right
      
      memcpy(cur_row_bak, cur_row, 64 * sizeof(float));

      // row additions
      // Add previous and following line
      float * restrict nxt_row = reverse ? cur_row - 64 : cur_row + 64;
      for (size_t col = 0; col < 64; col++) {
        tmp = prev_row_bak[col] + nxt_row[col] + row_buffer3[col];
        cur_row[col] = tmp * 0.1f;
      }

      // switch buffers
      tmp2 = prev_row_bak;
      prev_row_bak = cur_row_bak;
      cur_row_bak = tmp2;
    }
  }
}




/* 
 * Compute upper and left border into two fields (cache reasons) with full black tile on top left
 */
struct float_ptr_pair precompute_border(const size_t niters, const size_t offset, bool reverse) {
  static float row_buffer[64] __attribute__((aligned(128)));

  // Adding a padding to the border allows us to circumvent some expensive modulo operations
  const size_t border_size                 = niters;
  const size_t border_size_padding_bits    = 32 - __builtin_clz (border_size-1); // round up to the next power of 2
  const size_t border_size_with_padding    = 1 << border_size_padding_bits;

  // Determine aligned field sizes
  const size_t vert_field_size            = (((2 * border_size + 1)    *  64 * sizeof(float)) + (BORDER_FIELD_ALIGNMENT - 1)) & (~(unsigned long)(BORDER_FIELD_ALIGNMENT - 1));
  const size_t horiz_field_size           = ((border_size_with_padding *  64 * sizeof(float)) + (BORDER_FIELD_ALIGNMENT - 1)) & (~(unsigned long)(BORDER_FIELD_ALIGNMENT - 1));
  const size_t vert_full_field_size       = ((border_size              * 128 * sizeof(float)) + (BORDER_FIELD_ALIGNMENT - 1)) & (~(unsigned long)(BORDER_FIELD_ALIGNMENT - 1));
  const size_t horiz_full_field_size      = ((border_size_with_padding * 128 * sizeof(float)) + (BORDER_FIELD_ALIGNMENT - 1)) & (~(unsigned long)(BORDER_FIELD_ALIGNMENT - 1));


  // Allocate the memory
  float * const restrict vert_field       = (float *)__builtin_assume_aligned(mmap(NULL,
                                                              vert_field_size + horiz_field_size + vert_full_field_size + horiz_full_field_size,
                                                              PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0),           BORDER_FIELD_ALIGNMENT);
  float * const restrict horiz_field      = (float *)__builtin_assume_aligned((uint8_t *)vert_field      + vert_field_size,      BORDER_FIELD_ALIGNMENT);
  float * const restrict vert_full_field  = (float *)__builtin_assume_aligned((uint8_t *)horiz_field     + horiz_field_size,     BORDER_FIELD_ALIGNMENT);
  float * const restrict horiz_full_field = (float *)__builtin_assume_aligned((uint8_t *)vert_full_field + vert_full_field_size, BORDER_FIELD_ALIGNMENT);

  if (!vert_field) {
    fprintf(stderr, "Memory Error\n");
    exit(-1);
  }


  // TODO: do this without a buffer
  // setup chessboard pattern
  for (size_t i = 32; i < 64; i++) { row_buffer[i]  = WHITE_FLOAT; }

  const size_t rowlim = 2 * border_size + 1;
  for (size_t row = 0; row < rowlim; row++) {
    float * restrict cur_row = vert_field + (row << 6);

    // TODO: unroll
    for (size_t col = 0; col < 64; col++) {
       cur_row[col] = (row + (reverse ? (offset-border_size) : offset)) & 64 ? row_buffer[63-col] : row_buffer[col];
    }
  }


  // stencil vertical_field
  stencil_border_part(border_size, vert_field, reverse);

  // create horizontal image, expensive but better in the long run
  float * restrict vert_field_ptr = reverse ? vert_field + ((border_size + 1) << 6) : vert_field; // we may need to shift rows in the reverse case

  for (size_t row = 0; row < 64; row++) {
    float * restrict cur_row = horiz_field + (row << border_size_padding_bits);
    for (size_t col = 0; col < border_size; col++) {
      cur_row[col] = vert_field_ptr[(col<<6) + row];
    }
  }
  
  
  // fill full images
  for (size_t row = 0; row < border_size; row++) {
    float * restrict cur_row_write = vert_full_field + (row<<7);
    for (size_t col = 0; col < 128; col++) {
      // TODO: unroll
      cur_row_write[col] = vert_field_ptr[(row<<6) + (col < 32 ? 31-col : (col < 96 ? col-32: 159-col))];
    }
  }

  // fill full images
  for (size_t row = 0; row < 128; row++) {
    float * restrict cur_row_write = horiz_full_field + (row << border_size_padding_bits);
    for (size_t col = 0; col < border_size; col++) {
      // TODO: unroll
      cur_row_write[col] = horiz_field[((row < 32 ? 31 - row : (row < 96 ? row-32:159-row)) << border_size_padding_bits) + col];
    }
  }

  //dump_field("test.pgm", border_size_with_padding, 128, horiz_full_field);
  //dump_field("test.pgm", 128, border_size, vert_full_field);
  //dump_field("test.pgm", border_size_with_padding, 64, horiz_field);
  //dump_field("test.pgm", 64, 2 * border_size - 1, vert_field);

  return (struct float_ptr_pair) {.ptr1 = vert_full_field, .ptr2 = horiz_full_field};
}



/*
 * black: whether the top-left tile is black
 * returns a 2*niters+1 x 2*niters+1 big field, but only assures, that the top left niters x niters is correct
 * TODO: test padding 
 */
float *precompute_symmetric_edge(const size_t niters, bool black) {
  const size_t border_size = niters;
  const size_t width = 2 * border_size + 1;

  static float * restrict row_buffer1;
  static float * restrict row_buffer2;
  static float * restrict row_buffer3;
  static void * all_ptr = NULL;

  float * restrict prev_row_bak, * restrict cur_row_bak, * restrict tmp2;
  float tmp;


  // allocate stuff
  float * const restrict field = (float *)__builtin_assume_aligned(mmap(NULL,
                                                              width * width * sizeof(float),
                                                              PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0),BORDER_FIELD_ALIGNMENT);

  if (!field) {
    fprintf(stderr, "Memory Error %d\n", __LINE__);
    exit(-1);
  }

  if (!row_buffer1) {
    if (posix_memalign(&all_ptr, 0x100, 3*width*sizeof(float))) { // will never be freed
      fprintf(stderr, "Memory Error %d\n", __LINE__);
      exit(-1);
    }
    row_buffer1 = all_ptr;
    row_buffer2 = (float *)all_ptr + width;
    row_buffer3 = row_buffer2 + width;
  }

  // setup chessboard pattern
  for (size_t row = 0; row < width; row++) {
    float * restrict cur_row = field + (row * width);

    if (black) {
      for (size_t col = 0; col < width; col++) {
         cur_row[col] = ((col & 64) ^ (row & 64)) ? WHITE_FLOAT : 0.0f;
      }
    } else {
      for (size_t col = 0; col < width; col++) {
         cur_row[col] = ((col & 64) ^ (row & 64)) ? 0.0f : WHITE_FLOAT;
      }
    }
  }



  // stencil
  prev_row_bak = row_buffer1;
  cur_row_bak  = row_buffer2;

  for (size_t num_rows = width - 1; num_rows >= border_size; num_rows--) {
    memset(prev_row_bak, 0, sizeof(float) * width);

    for (size_t row = 0; row < num_rows; row++) {
      float * restrict cur_row = field + (row * width);

      memcpy(cur_row_bak, cur_row, width * sizeof(float));

      // stencil over the horizontal
      row_buffer3[0] = cur_row[0] * 6.0f + cur_row[1];
      for (size_t col = 1; col < row; col++) {
        row_buffer3[col] = cur_row[col-1] + cur_row[col] * 6.0f + cur_row[col+1];
      }

      // row additions
      // Add previous and following line
      float * restrict nxt_row = cur_row + width;
      for (size_t col = 0; col < row; col++) {
        tmp = prev_row_bak[col] + nxt_row[col] + row_buffer3[col];
        cur_row[col] = tmp * 0.1f;
      }

      // last field is a special case
      cur_row[row] = ((row == 0 ? 0.0f : 2 * cur_row_bak[row-1]) + cur_row[row] * 6.0f + 2.0f * cur_row[row+width]) * 0.1f;

      // switch buffers
      tmp2 = prev_row_bak;
      prev_row_bak = cur_row_bak;
      cur_row_bak = tmp2;
    }
  }

  // mirror over the diagonal
  for (size_t row = 0; row < width; row++) {
    float * restrict cur_row = field + row * width;
    for (size_t col = row; col < width; col++) {
      cur_row[col] = field[col * width + row];
    }
  }

  //dump_field("test.pgm", width, width, field);

  return field;
}








// ========================= DEBUGGING =============================
void print_row(float *row, size_t length) {
  for(int i = 0; i < length; i++) {
    putchar(row[i]>50.0?'o':'x'); // o is light x is dark
  }
  putchar('\n');
}


void dump_field(char *fname, int nx, int ny, float *field) {
  float *tmp = malloc(sizeof(float) * (nx + 2) * (ny+2));
  for (size_t row = 0; row < ny; row++) {
    for(size_t col = 0; col < nx; col++) {
      tmp[(row+1)*(nx+2) + col+1] = field[row*nx+col];
    }
  }
  output_image(fname, nx, ny,
                  nx+2, ny+2, (float *)tmp);
}


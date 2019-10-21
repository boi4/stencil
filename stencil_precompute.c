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


/* This function will stencil a quarter of a tile in the center
 * it will simulate an infinite space by wrapping around the corners
 */
float *precompute_center(size_t niters) {
  static float center_small[32][32]; // [row][column], compiler will make two elements on same row be next to each other
  static float center_small_white[32][32];
  static float center_big[128][128]; // will be computed out of center_small

  static float row_buffer1[32];
  static float row_buffer2[32];
  static float row_buffer3[32];

  float tmp;
  float * restrict prev_row_bak, * restrict cur_row_bak, * restrict tmp2;

  prev_row_bak = row_buffer1;
  cur_row_bak  = row_buffer2;

  for (size_t i = 0; i < niters; i++) {
    for (size_t row = 0; row < 32; row++) {
      float * restrict cur_row = center_small[row];
      row_buffer3[ 0] = cur_row[0] * 7.0f + cur_row[1];
      for (size_t col = 1; col < 31; col++) {
        row_buffer3[col] = cur_row[col] * 6.0f + cur_row[col+1] + cur_row[col-1];
      }
      row_buffer3[31] = WHITE_FLOAT + cur_row[31] * 5.0f + cur_row[30];

      // row additions
      // Add previous and following line
      if (row == 0) {
        memcpy(cur_row_bak, cur_row, 32 * sizeof(float));
        float * restrict nxt_row = cur_row + 32;
        for (size_t col = 0; col < 32; col++) {
          tmp = cur_row_bak[col] + nxt_row[col] + row_buffer3[col];
          center_small[row][col] = tmp * 0.1f;
        }
      } else if (row == 31) {
        for (size_t col = 0; col < 32; col++) {
          tmp = prev_row_bak[col] + (WHITE_FLOAT - cur_row[col]) + row_buffer3[col];
          center_small[row][col] = tmp * 0.1f;
        }
      } else {
        memcpy(cur_row_bak, cur_row, 32 * sizeof(float));
        float * restrict nxt_row = cur_row + 32;
        for (size_t col = 0; col < 32; col++) {
          tmp = prev_row_bak[col] + nxt_row[col] + row_buffer3[col];
          center_small[row][col] = tmp * 0.1f;
        }
      }

      // switch buffers
      tmp2 = prev_row_bak;
      prev_row_bak = cur_row_bak;
      cur_row_bak = tmp2;
    }
  }

  // copy white field into middle
  for (size_t row = 0; row < 32; row++) {
    for (size_t col = 0; col < 32; col++) {
      center_small_white[row][col] = WHITE_FLOAT - center_small[row][col];
    }
  }

  // fill big field

  // go from top left to bottom right
  // TODO: unroll
  for (size_t row = 0; row < 128; row++) {
    float * restrict cur_row = ((row & 64) ? center_small_white : center_small)
                               [(row & 32) ? row & 0x1f: ~row & 0x1f];
    for (size_t col = 0; col < 64; col++) {
      center_big[row][col] = cur_row[col & 32 ? col & 0x1f : ~col & 0x1f] ;
    }
    cur_row = ((row & 64) ? center_small : center_small_white)
                               [(row & 32) ? row & 0x1f: ~row & 0x1f];
    for (size_t col = 64; col < 128; col++) {
      center_big[row][col] = cur_row[col & 32 ? col & 0x1f : ~col & 0x1f] ;
    }
  }

  //dump_field("test.pgm", 32, 32, center_small[0]);
  //dump_field("test.pgm", 128, 128, center_big[0]);
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

  if (border_size == 0) return;

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
 * TODO: add padding, vectorize
 */
float *precompute_symmetric_edge(const size_t niters, bool black) {
  const size_t border_size = niters;
  const size_t width_used = 2 * border_size + 1;
  const size_t width = (width_used + (EDGE_ALIGNMENT/sizeof(float)) - 1) & ~((EDGE_ALIGNMENT/sizeof(float)) - 1);

  static float * restrict row_buffer1;
  static float * restrict row_buffer2;
  static float * restrict row_buffer3;
  static void * all_ptr = NULL;

  float * restrict prev_row_bak, * restrict cur_row_bak, * restrict tmp2;
  float tmp;

  if (border_size == 0) return NULL;


  // allocate stuff
  float * const restrict field = (float *)__builtin_assume_aligned(mmap(NULL,
                                                              width * width_used * sizeof(float),
                                                              PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0), EDGE_ALIGNMENT);

  if (!field) {
    fprintf(stderr, "Memory Error %d\n", __LINE__);
    exit(-1);
  }

  if (!row_buffer1) {
    if (posix_memalign(&all_ptr, 32, 3 * width*sizeof(float))) { // will never be freed
      fprintf(stderr, "Memory Error %d\n", __LINE__);
      exit(-1);
    }
    row_buffer1 = __builtin_assume_aligned(all_ptr, EDGE_ALIGNMENT);
    row_buffer2 = __builtin_assume_aligned((float *)all_ptr + width, EDGE_ALIGNMENT);
    row_buffer3 = __builtin_assume_aligned(row_buffer2 + width, EDGE_ALIGNMENT);
  }

  // setup chessboard pattern
  for (size_t row = 0; row < width; row++) {
    float * restrict cur_row = __builtin_assume_aligned(field + (row * width), EDGE_ALIGNMENT);

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

  for (size_t num_rows = width_used - 1; num_rows >= border_size; num_rows--) {
    memset(prev_row_bak, 0, sizeof(float) * width);

    for (size_t row = 0; row < num_rows; row++) {
      float * restrict cur_row = __builtin_assume_aligned(field + (row * width), EDGE_ALIGNMENT);

      memcpy(cur_row_bak, cur_row, width_used * sizeof(float));

      // stencil over the horizontal
      row_buffer3[0] = cur_row[0] * 6.0f + cur_row[1];
      for (size_t col = 1; col < row; col++) {
        row_buffer3[col] = cur_row[col-1] + cur_row[col] * 6.0f + cur_row[col+1];
      }

      // row additions
      // Add previous and following line
      float * restrict const nxt_row = __builtin_assume_aligned(cur_row + width, EDGE_ALIGNMENT);
      float * restrict const prev    = __builtin_assume_aligned(prev_row_bak, EDGE_ALIGNMENT);
      float * restrict const rb3     = __builtin_assume_aligned(row_buffer3, EDGE_ALIGNMENT);
      for (size_t col = 0; col < row; col++) {
        tmp = prev[col] + nxt_row[col] + rb3[col];
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
  for (size_t row = 0; row < width_used; row++) {
    float * restrict cur_row = field + row * width;
    for (size_t col = row; col < width_used; col++) {
      cur_row[col] = field[col * width + row];
    }
  }

  //dump_field("test.pgm", width, width, field);

  return field;
}




// used only when nx % 128 != 0 or ny % 128 != 0
float *precompute_full_edge(const size_t niters, const size_t xoff, const size_t yoff) {
  const size_t border_size = niters;
  const size_t height = 2 * border_size + 1;
  const size_t width = (height + (EDGE_ALIGNMENT/sizeof(float)) - 1) & ~((EDGE_ALIGNMENT/sizeof(float)) - 1);

  static float * restrict row_buffer1;
  static float * restrict row_buffer2;
  static float * restrict row_buffer3;
  static void * all_ptr = NULL;

  float * restrict prev_row_bak, * restrict cur_row_bak, * restrict tmp2;
  float tmp;

  if (border_size == 0) return NULL;


  // allocate stuff
  float * const restrict field = (float *)__builtin_assume_aligned(mmap(NULL,
                                                              width * height * sizeof(float),
                                                              PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0), EDGE_ALIGNMENT);

  if (!field) {
    fprintf(stderr, "Memory Error %d\n", __LINE__);
    exit(-1);
  }

  if (!row_buffer1) {
    if (posix_memalign(&all_ptr, 32, 3 * width*sizeof(float))) { // will never be freed
      fprintf(stderr, "Memory Error %d\n", __LINE__);
      exit(-1);
    }
    row_buffer1 = __builtin_assume_aligned(all_ptr, EDGE_ALIGNMENT);
    row_buffer2 = __builtin_assume_aligned((float *)all_ptr + width, EDGE_ALIGNMENT);
    row_buffer3 = __builtin_assume_aligned(row_buffer2 + width, EDGE_ALIGNMENT);
  }

  // setup chessboard pattern
  for (size_t row = yoff; row < height + yoff; row++) {
    float * restrict cur_row = __builtin_assume_aligned(field + ((row-yoff) * width), EDGE_ALIGNMENT);

    for (size_t col = xoff; col < height + xoff; col++) {
       cur_row[col-xoff] = ((col & 64) ^ (row & 64)) ? WHITE_FLOAT : 0.0f;
    }
  }

  // stencil
  prev_row_bak = row_buffer1;
  cur_row_bak  = row_buffer2;

  for (size_t num_rows = height - 1; num_rows >= border_size; num_rows--) {
    memset(prev_row_bak, 0, sizeof(float) * width);

    for (size_t row = 0; row < num_rows; row++) {
      float * restrict cur_row = __builtin_assume_aligned(field + (row * width), EDGE_ALIGNMENT);

      memcpy(cur_row_bak, cur_row, height * sizeof(float));

      // stencil over the horizontal
      row_buffer3[0] = cur_row[0] * 6.0f + cur_row[1];
      for (size_t col = 1; col < height; col++) {
        row_buffer3[col] = cur_row[col-1] + cur_row[col] * 6.0f + cur_row[col+1];
      }

      // row additions
      // Add previous and following line
      float * restrict const nxt_row = __builtin_assume_aligned(cur_row + width, EDGE_ALIGNMENT);
      float * restrict const prev    = __builtin_assume_aligned(prev_row_bak, EDGE_ALIGNMENT);
      float * restrict const rb3     = __builtin_assume_aligned(row_buffer3, EDGE_ALIGNMENT);
      for (size_t col = 0; col < height; col++) {
        tmp = prev[col] + nxt_row[col] + rb3[col];
        cur_row[col] = tmp * 0.1f;
      }

      // last field is a special case
      cur_row[height] = ((row == 0 ? 0.0f : 2 * cur_row_bak[height-1]) + cur_row[height] * 6.0f + 2.0f * cur_row[row+width]) * 0.1f;

      // switch buffers
      tmp2 = prev_row_bak;
      prev_row_bak = cur_row_bak;
      cur_row_bak = tmp2;
    }
  }

  //dump_field("test.pgm", width, height, field);

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


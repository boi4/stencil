#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/mman.h>


extern void output_image(const char* file_name, const int nx, const int ny,
                  const int width, const int height, float* image);


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

// TODO: use chessboard instead of new fields
// TODO: use smaller fields


float center_small[64][64]; // [row][column], compiler will make two elements on same row be next to each other
float center_big[128][128]; // will be computed out of center_small

/* This function will stencil the part where four tiles meet
 * it will simulate an infinite space by wrapping around the corners
 * probably could be optimized by using the symmetry following the diagonals (using only one fourth of small_field)
 */
void precompute_center(size_t niters) {
  static float row_buffer[64]; // holds the previous row
  static float row_buffer2[64]; // holds the previous row

  float sum, cur, nxt, tmp;
  float * restrict prev_row_bak, * restrict cur_row_bak, * restrict tmp2;

  // create chessboard pattern
  for (size_t i = 0; i < 32; i++) {
    row_buffer[i] = 100.0f;
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
    row_buffer[i] = 100.0f;
  }
  for (size_t i = 0; i < 32; i++) {
      memcpy(center_small[i], row_buffer, sizeof(row_buffer));
  }
  // we first do the lower half because we don't have to init row_buffer again


  prev_row_bak = row_buffer;
  cur_row_bak = row_buffer2;

  for (size_t i = 0; i < niters; i++) {
    for (size_t row = 0; row < 63; row++) {
      cur = center_small[63-row][63];
      nxt = center_small[row][0];
      for(size_t col = 0; col < 63; col++) {
        sum  = cur; // add previous field
        cur_row_bak[col] = cur;
        sum += nxt * 6.0f; // add current field
        cur  = nxt;
        nxt  = center_small[row][col+1];
        sum += nxt; // add next field
        center_small[row][col] = sum; // TODO: maybe write this into other field
      }
      // special case for last column to avoid some modulo
      sum  = cur; // add previous field
      sum += nxt * 6.0f; // add current field
      cur  = nxt;
      nxt  = center_small[63-row][0]; // TODO: this may use a wrong value
      sum += nxt; // add next field
      center_small[row][63] = sum;


      // row additions
      // TODO: vecorize
      // Add previous and following line
      float * restrict nxt_row = center_small[row + 1];
      for (size_t col = 0; col < 64; col++) {
        tmp = prev_row_bak[col] + nxt_row[col] + center_small[row][col];
        center_small[row][col] = tmp * 0.1f;
      }

      // switch buffers
      tmp2 = prev_row_bak;
      prev_row_bak = cur_row_bak;
      cur_row_bak = tmp2;
    }

    // special case last row
    cur = center_small[0][63];
    nxt = center_small[63][0];
    for(size_t col = 0; col < 63; col++) {
      sum  = cur; // add previous field
      sum += nxt * 6.0f; // add current field
      cur  = nxt;
      nxt  = center_small[63][col+1];
      sum += nxt; // add next field
      center_small[63][col] = sum;
    }
    // special case to avoid some modulo
    sum  = cur; // add previous field
    sum += nxt * 6.0f; // add current field
    cur  = nxt;
    nxt  = center_small[0][0]; // TODO: this will use a wrong value
    sum += nxt; // add next field
    center_small[63][63] = sum;

    // TODO: vectorize
    // Add previous and following line
    float * restrict nxt_row = center_small[0];
    for (size_t col = 0; col < 64; col++) {
      tmp = prev_row_bak[col] + nxt_row[63-col] + center_small[63][col];
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
}




/* Compute upper and left border into two fields (cache reasons) with full black tile on top left
 */
struct float_ptr_pair {
  float * ptr1;
  float * ptr2;
};


typedef double aligned_float __attribute__((aligned (128)));

void stencil_border_part(size_t border_size,
                         float * const restrict vert_field_arg,
                         bool reverse // wether to go from the bottom to the top
                         ) {

  float *vert_field = __builtin_assume_aligned(vert_field_arg, 128);
  float tmp;

  static float row_buffer[64]  __attribute__ ((aligned (128)));
  static float row_buffer2[64] __attribute__ ((aligned (128)));
  static float row_buffer3[64] __attribute__ ((aligned (128)));

  float * restrict prev_row_bak, * restrict cur_row_bak, * restrict tmp2;

  prev_row_bak = row_buffer;
  cur_row_bak = row_buffer2;

  // start stencilin'
  for (size_t num_rows = 2 * border_size - 1; num_rows >= border_size; num_rows--) {

    for (size_t i = 0; i < 64; i++) { prev_row_bak[i] = 0.0f; } // simulate first row, which is black

    for (size_t row = 0; row < num_rows; row++) {
      float * restrict cur_row = &vert_field[reverse ? (2 * border_size - row) << 7: row<<7];

      // stencil over the horizontal
      memcpy(cur_row_bak, cur_row, 64);

      cur_row[ 0] = cur_row_bak[ 0] * 7.0f + cur_row_bak[ 1];
      cur_row[63] = cur_row_bak[63] * 7.0f + cur_row_bak[62];
      for (size_t col = 1; col < 63; col++) {
        cur_row[col] = cur_row_bak[col] * 6.0f + cur_row_bak[col+1] + cur_row_bak[col-1];
      }

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





struct float_ptr_pair precompute_upper_border(const size_t niters) {
  float row_buffer[64]; // holds the previous row
  float row_buffer2[64]; // holds the previous row

  const size_t border_size = niters;

  // Allocate both fields
  // the vertical field is two times as big as the the last pixel of the border will be affected by the pixel at 2*border+1
  const size_t vert_field_size = (((2 * border_size + 1) * 64 * sizeof(float)) + 0x1000) & (~(unsigned long)0xfff);
  // the horizontal field will be just the vertical field copied over once
  const size_t horiz_field_size = ((border_size * 64 * sizeof(float)) + 0x1000) & (~(unsigned long)0xfff);

  float * const restrict vert_field = (float *)mmap(NULL, vert_field_size + horiz_field_size,
                               PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS,
                               -1, 0);
  float * const restrict horiz_field = (float *)((char *)vert_field + vert_field_size);
  if (!vert_field) {
    fprintf(stderr, "Memory Error\n");
    exit(-1);
  }

  // setup chessboard pattern
  for (size_t i =  0; i < 32; i++) { row_buffer[i]  =   0.0f; }
  for (size_t i = 32; i < 64; i++) { row_buffer[i]  = 100.0f; }
  for (size_t i =  0; i < 32; i++) { row_buffer2[i] = 100.0f; }
  for (size_t i = 32; i < 64; i++) { row_buffer2[i] =   0.0f; }

  for (size_t row = 0; row < 2 * border_size + 1; row++) {
    for (size_t col = 0; col < 64; col++) {
      vert_field[(row << 6) + col] = row & 32 ? row_buffer2[col] : row_buffer[col];
    }
  }


  stencil_border_part(border_size, vert_field, false);

//  // create horizontal image, very expensive but better in the long run
//  for (size_t row = 0; row < 128; row++) {
//    float * restrict cur_row = &horiz_field[row * border_size];
//    for (size_t col = 0; col < border_size; col++) {
//      cur_row[col] = vert_field[(col<<7) + row];
//    }
//  }
  
  dump_field("test.pgm", 64, border_size, vert_field);

  return (struct float_ptr_pair) {.ptr1 = vert_field, .ptr2 = horiz_field};
}



/*
 * This function computes the lower or the right border. It returns a float ptr struct.
 * ptr1 is lower border in vertical, ptr2 is right border in horizontal
 * length should be the either the width or the heigth of the whole chessboard (without the invinsible borders)
 * if width and height are equal, one function call should be enough (could be optimized)
 */
struct float_ptr_pair precompute_lower_border(const size_t niters, const size_t length) {
  float row_buffer[128]; // holds the previous row
  float row_buffer2[128]; // holds the previous row

  const size_t border_size = niters;

  // Allocate both fields
  // the vertical field is two times as big as the the last pixel of the border will be affected by the pixel at 2*border+1
  const size_t vert_field_size = (((2 * border_size + 1) * 128 * sizeof(float)) + 0x1000) & (~(unsigned long)0xfff);
  // the horizontal field will be just the vertical field copied over once
  const size_t horiz_field_size = ((border_size * 128 * sizeof(float)) + 0x1000) & (~(unsigned long)0xfff);

  float * restrict vert_field = (float *)mmap(NULL, vert_field_size + horiz_field_size,
                               PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS,
                               -1, 0);
  float * const restrict horiz_field = (float *)((char *)vert_field + vert_field_size);
  if (!vert_field) {
    fprintf(stderr, "Memory Error\n");
    exit(-1);
  }

  // setup chessboard pattern
  for (size_t i =  0; i <  64; i++) { row_buffer[i] = 0.0f; }
  for (size_t i = 64; i < 128; i++) { row_buffer[i] = 100.0f; }
  for (size_t i =  0; i <  64; i++) { row_buffer2[i] = 100.0f; }
  for (size_t i = 64; i < 128; i++) { row_buffer2[i] = 0.0f; }

  for (size_t row = 0; row < 2 * border_size + 1; row++) {
    for (size_t col = 0; col < 128; col++) {
      vert_field[(row << 7) + col] = (length - 2 * border_size - 1 + row) & 64 ? row_buffer2[col] : row_buffer[col];
    }
  }


  stencil_border_part(border_size, vert_field, true);
  vert_field = vert_field + 128 * border_size;

  // create horizontal image, very expensive but better in the long run
  for (size_t row = 0; row < 128; row++) {
    float * restrict cur_row = &horiz_field[row * border_size];
    for (size_t col = 0; col < border_size; col++) {
      cur_row[col] = vert_field[(col<<7) + row];
    }
  }
  
  dump_field("test.pgm", 128, border_size, vert_field);

  return (struct float_ptr_pair) {.ptr1 = vert_field, .ptr2 = horiz_field};
}




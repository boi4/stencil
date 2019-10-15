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

/* Compute upper and left border into two fields (cache reasons) with full black tile on top left
 */
struct float_ptr_pair {
  float * ptr1;
  float * ptr2;
};



void stencil_border_part(size_t border_size,
                         float * const restrict horiz_field // should be 128 wide and 2*bordersize+1 long
                         ) {

  const size_t width = 2 * border_size + 1;

  float sum, cur, nxt, first, tmp;
  float * restrict prev_row_bak, * restrict cur_row_bak, * restrict tmp2;

  static float * restrict row_buffer  = NULL;
  static float * restrict row_buffer2 = NULL;
  static float * restrict row_buffer3 = NULL;
  if (row_buffer == NULL) {
    row_buffer  = malloc(width * sizeof(float));
    row_buffer2 = malloc(width * sizeof(float));
    row_buffer3 = malloc(width * sizeof(float));
  }

  prev_row_bak = row_buffer;
  cur_row_bak = row_buffer2;

  // start stencilin'
  for (size_t num_cols = width; num_cols >= border_size; num_cols--) {

    for (size_t row = 0; row < 64; row++) {
      float * restrict cur_row = horiz_field + row * width;

      // stencil over the horizontal
      cur = 0.0f; // black "hidden" border
      nxt = cur_row[0];
      for (size_t col = 0; col < num_cols; col++) {
        sum  = cur; // add previous field
        cur_row_bak[col] = cur;
        sum += nxt * 6.0f; // add current field
        cur  = nxt;
        nxt  = cur_row[col + 1];
        sum += nxt; // add next field

        row_buffer3[col] = sum;
      }

      // row additions
      // Add previous and following line
      float * restrict nxt_row = (row == 63) ? cur_row : cur_row + width;
      if (row == 0) {
        for (size_t col = 0; col < num_cols; col++) {
          tmp = row_buffer3[col];
          tmp += cur_row_bak[col];
          tmp += nxt_row[col];
          cur_row[col] = tmp * 0.1f;
        }
      } else {
        for (size_t col = 0; col < num_cols; col++) {
          tmp = prev_row_bak[col] + nxt_row[col] + row_buffer3[col];
          cur_row[col] = tmp * 0.1f;
        }
      }

      // switch buffers
      tmp2 = prev_row_bak;
      prev_row_bak = cur_row_bak;
      cur_row_bak = tmp2;
    }
  }
}





struct float_ptr_pair precompute_upper_border(const size_t niters) {
  const size_t border_size = niters;
  const size_t width = 2 * border_size + 1;

  float row_buffer[width]; // holds the previous row
  float row_buffer2[width]; // holds the previous row

  // Allocate both fields
  // the vertical field is two times as big as the the last pixel of the border will be affected by the pixel at 2*border+1
  const size_t horiz_field_size = ((width * 128 * sizeof(float)) + 0x1000) & (~(unsigned long)0xfff);
  // the horizontal field will be just the vertical field copied over once
  const size_t vert_field_size = ((border_size * 128 * sizeof(float)) + 0x1000) & (~(unsigned long)0xfff);

  float * const restrict horiz_field = (float *)mmap(NULL, vert_field_size + horiz_field_size,
                               PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS,
                               -1, 0);
  float * const restrict vert_field = (float *)((char *)horiz_field + horiz_field_size);
  if (!horiz_field) {
    fprintf(stderr, "Memory Error\n");
    exit(-1);
  }

  // setup chessboard pattern
  for (size_t i = 0; i < width; i++) { row_buffer[i]  = (i & 64) ? 0.0f : 100.0f; }
  for (size_t i = 0; i < width; i++) { row_buffer2[i] = (i & 64) ? 100.0f : 0.0f; }

  for (size_t row = 0; row < 64; row++) {
    for (size_t col = 0; col < width; col++) {
      horiz_field[width * row + col] = row & 32 ? row_buffer[col] : row_buffer2[col];
    }
  }


  stencil_border_part(border_size, horiz_field);

//  // create horizontal image, very expensive but better in the long run
//  for (size_t row = 0; row < 128; row++) {
//    float * restrict cur_row = &horiz_field[row * border_size];
//    for (size_t col = 0; col < border_size; col++) {
//      cur_row[col] = vert_field[(col<<7) + row];
//    }
//  }
  
  //dump_field("test.pgm", width, 64, horiz_field);

  return (struct float_ptr_pair) {.ptr1 = vert_field, .ptr2 = horiz_field};
}

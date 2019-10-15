#include <stddef.h>
#include <stdio.h>
#include <string.h>

float field_small[64][64]; // [row][column], compiler will make two elements on same row be next to each other
float field_big[128][128]; // will be computed out of field_small
float row_buffer[64]; // holds the previous row
float row_buffer2[64]; // holds the previous row

extern void output_image(const char* file_name, const int nx, const int ny,
                  const int width, const int height, float* image);


void print_row(float *row) {
  for(int i = 0; i < 64; i++) {
    putchar(row[i]>50.0?'o':'x'); // o is light x is dark
  }
  putchar('\n');
}

/* This function will stenctil the part where four tiles meet
 * it will simulate an infinite space by wrapping around the corners
 * probably could be optimized by using the symmetry following the diagonals
 */
void stencil_four_tiles(size_t niters) {
  float sum, cur, nxt, tmp;
  float * restrict prev_row, * restrict cur_row, * restrict tmp2;

  // create chessboard pattern
  for (size_t i = 0; i < 32; i++) {
    row_buffer[i] = 100.0f;
  }
  for (size_t i = 32; i < 64; i++) {
    row_buffer[i] = 0.0f;
  }
  for (size_t i = 32; i < 64; i++) {
      memcpy(field_small[i], row_buffer, sizeof(row_buffer));
  }
  for (size_t i = 0; i < 32; i++) {
    row_buffer[i] = 0.0f;
  }
  for (size_t i = 32; i < 64; i++) {
    row_buffer[i] = 100.0f;
  }
  for (size_t i = 0; i < 32; i++) {
      memcpy(field_small[i], row_buffer, sizeof(row_buffer));
  }
  // we first do the lower half because we don't have to init row_buffer again


  // switch this one around
  prev_row = row_buffer;
  cur_row = row_buffer2;

  for (size_t i = 0; i < niters; i++) {
    for (size_t row = 0; row < 63; row++) {
      // backup current row
      memcpy(cur_row, field_small[row], sizeof(row_buffer));

      cur = field_small[63-row][63];
      nxt = field_small[row][0];
      for(size_t col = 0; col < 63; col++) {
        sum  = cur; // add previous field_small
        sum += nxt * 6.0f; // add current field_small
        cur  = nxt;
        nxt  = field_small[row][col+1];
        sum += nxt; // add next field_small
        field_small[row][col] = sum;
      }
      // special case for last column to avoid some modulo
      sum  = cur; // add previous field_small
      sum += nxt * 6.0f; // add current field_small
      cur  = nxt;
      nxt  = field_small[63-row][0];
      sum += nxt; // add next field_small
      field_small[row][63] = sum;


      // row additions
      // TODO: vecorize
      // Add previous and following line
      float * restrict nxt_row = field_small[row + 1];
      for (size_t col = 0; col < 64; col++) {
        tmp = prev_row[col] + nxt_row[col] + field_small[row][col];
        field_small[row][col] = tmp * 0.1f;
      }

      // switch buffers
      tmp2 = prev_row;
      prev_row = cur_row;
      cur_row = tmp2;
    }

    // special case last row
    cur = field_small[0][63];
    nxt = field_small[63][0];
    for(size_t col = 0; col < 63; col++) {
      sum  = cur; // add previous field_small
      sum += nxt * 6.0f; // add current field_small
      cur  = nxt;
      nxt  = field_small[63][col+1];
      sum += nxt; // add next field_small
      field_small[63][col] = sum;
    }
    // special case to avoid some modulo
    sum  = cur; // add previous field_small
    sum += nxt * 6.0f; // add current field_small
    cur  = nxt;
    nxt  = field_small[0][0];
    sum += nxt; // add next field_small
    field_small[63][63] = sum;

    // TODO: vectorize
    // Add previous and following line
    float * restrict nxt_row = field_small[0];
    for (size_t col = 0; col < 64; col++) {
      tmp = prev_row[col] + nxt_row[63-col] + field_small[63][col];
      field_small[63][col] = tmp * 0.1f;
    }

    // set reverse row for the first line in next iteration
    for(size_t i = 0; i < 64; i++) {cur_row[i] = field_small[63][63-i];}

    // switch buffers
    tmp2 = prev_row;
    prev_row = cur_row;
    cur_row = tmp2;
  }


  // fill big field
  // copy small field into the middle
  for (size_t row = 32; row < 96; row++) {
    for (size_t col = 32; col < 96; col++) {
      field_big[row][col] = field_small[row-32][col-32];
    }
  }

  // mirror small field on the left
  for (size_t row = 32; row < 96; row++) {
    for (size_t col = 0; col < 32; col++) {
      field_big[row][col] = field_big[row][63-col];
    }
  }

  // mirror small field on the right
  for (size_t row = 32; row < 96; row++) {
    for (size_t col = 96; col < 128; col++) {
      field_big[row][col] = field_big[127-row][col-64];
    }
  }

  // mirror whole thing on top
  for (size_t row = 0; row < 32; row++) {
    for (size_t col = 0; col < 128; col++) {
      field_big[row][col] = field_big[63-row][col];
    }
  }

  // mirror whole thing on bottom
  for (size_t row = 96; row < 128; row++) {
    for (size_t col = 0; col < 128; col++) {
      field_big[row][col] = field_big[row-64][127-col];
    }
  }

  //// for debugging
  //static float field_small2[66][66] = { 0 };
  //for (size_t row = 0; row < 64; row++) {
  //  for(size_t col = 0; col < 64; col++) {
  //    field_small2[row+1][col+1] = field_small[row][col];
  //  }
  //}
  //output_image("test.pgm", 64, 64,
  //                66, 66, (float *)field_small2);
//  static float field_big2[130][130] = { 0 };
//  for (size_t row = 0; row < 128; row++) {
//    for(size_t col = 0; col < 128; col++) {
//      field_big2[row+1][col+1] = field_big[row][col];
//    }
//  }
//  output_image("test_big.pgm", 128, 128,
//                  130, 130, (float *)field_big2);
}


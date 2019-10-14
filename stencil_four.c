#include <stddef.h>
#include <stdio.h>
#include <string.h>

float field[64][64]; // [row][column], compiler will make two elements on same row be next to each other
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
  float *prev_row, *cur_row, *tmp2;

  // create chessboard pattern
  for (size_t i = 0; i < 32; i++) {
    row_buffer[i] = 100.0f;
  }
  for (size_t i = 32; i < 64; i++) {
    row_buffer[i] = 0.0f;
  }
  for (size_t i = 32; i < 64; i++) {
      memcpy(field[i], row_buffer, sizeof(row_buffer));
  }
  for (size_t i = 0; i < 32; i++) {
    row_buffer[i] = 0.0f;
  }
  for (size_t i = 32; i < 64; i++) {
    row_buffer[i] = 100.0f;
  }
  for (size_t i = 0; i < 32; i++) {
      memcpy(field[i], row_buffer, sizeof(row_buffer));
  }
  // we first do the lower half because we don't have to init row_buffer again


  // switch this one around
  prev_row = row_buffer;
  cur_row = row_buffer2;

  for (size_t i = 0; i < niters; i++) {
    for (size_t row = 0; row < 63; row++) {
      // backup current row
      memcpy(cur_row, field[row], sizeof(row_buffer));

      cur = field[63-row][63];
      nxt = field[row][0];
      for(size_t col = 0; col < 63; col++) {
        sum  = cur; // add previous field
        sum += nxt * 6.0f; // add current field
        cur  = nxt;
        nxt  = field[row][col+1];
        sum += nxt; // add next field
        field[row][col] = sum;
      }
      // special case for last column to avoid some modulo
      sum  = cur; // add previous field
      sum += nxt * 6.0f; // add current field
      cur  = nxt;
      nxt  = field[63-row][0];
      sum += nxt; // add next field
      field[row][63] = sum;


      // row additions
      // TODO: vecorize
      // Add previous and following line
      float *nxt_row = field[row + 1];
      for (size_t col = 0; col < 64; col++) {
        tmp = prev_row[col] + nxt_row[col] + field[row][col];
        field[row][col] = tmp * 0.1;
      }

      // switch buffers
      tmp2 = prev_row;
      prev_row = cur_row;
      cur_row = tmp2;
    }

    // special case last row
    cur = field[0][63];
    nxt = field[63][0];
    for(size_t col = 0; col < 63; col++) {
      sum  = cur; // add previous field
      sum += nxt * 6.0f; // add current field
      cur  = nxt;
      nxt  = field[63][col+1];
      sum += nxt; // add next field
      field[63][col] = sum;
    }
    // special case to avoid some modulo
    sum  = cur; // add previous field
    sum += nxt * 6.0f; // add current field
    cur  = nxt;
    nxt  = field[0][0];
    sum += nxt; // add next field
    field[63][63] = sum;

    // TODO: vectorize
    // Add previous and following line
    float *nxt_row = field[0];
    for (size_t col = 0; col < 64; col++) {
      tmp = prev_row[col] + nxt_row[63-col] + field[63][col];
      field[63][col] = tmp * 0.1f;
    }

    for(size_t i = 0; i < 64; i++) {cur_row[i] = field[63][63-i];}

    // switch buffers
    tmp2 = prev_row;
    prev_row = cur_row;
    cur_row = tmp2;
  }

  static float field2[66][66] = { 0 };
  for (size_t row = 0; row < 64; row++) {
    for(size_t col = 0; col < 64; col++) {
      field2[row+1][col+1] = field[row][col];
    }
  }
  output_image("test.pgm", 64, 64,
                  66, 66, (float *)field2);
}


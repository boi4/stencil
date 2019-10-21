#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/time.h>
#include <sys/mman.h>

#include "stencil.h"


void stencil(const size_t nx, const size_t ny, const size_t width, const size_t height, const size_t niters, float* image);
void stencil_full(const size_t nx, const size_t ny, const size_t width, const size_t height, float* image, size_t niters);
void init_image(const int nx, const int ny, const int width, const int height, float* image);
void output_image(const char* file_name, const int nx, const int ny,
                  const int width, const int height, float* image);
double wtime(void);

// all externs are defined in stencil_precomupte.c
extern float *precompute_center(size_t niters);
extern float *precompute_symmetric_edge(size_t niters, bool black);
extern float *precompute_full_edge(const size_t niters, const size_t xoff, const size_t yoff);
extern struct float_ptr_pair precompute_border(size_t niters, size_t offset, bool reverse);

// debug stuff, also in stencil_precompute.c
extern void dump_field(char *fname, int nx, int ny, float *field);
extern void print_row(float *row, size_t length);




int main(int argc, char* argv[])
{
  // Check usage
  if (argc != 4) {
    fprintf(stderr, "Usage: %s nx ny niters\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  // Initiliase problem dimensions from command line arguments
  size_t nx     =   strtoul(argv[1], NULL, 0);
  size_t ny     =   strtoul(argv[2], NULL, 0);
  size_t niters = 2*strtoul(argv[3], NULL, 0);

  // we pad width, so each row is 128 bit aligned
  // stencil
  size_t width  = ((nx + 2) + MAIN_FIELD_ALIGNMENT - 1) & ~(MAIN_FIELD_ALIGNMENT - 1);
  size_t height = ny + 2;

  // Allocate the image
  // we allocate an extra page, to put the field so on the memory that actually the first 'seen' block is aligned
  size_t image_size = ((width * height * sizeof(float)) + 0x2000LU) & (~(size_t)0xfffLU);

  void * restrict all = (float *)mmap(NULL, image_size,
                               PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS,
                               -1, 0);

  if (!all) {
    fprintf(stderr, "Memory Error\n");
    exit(-1);
  }
  float * restrict image = (float *)((char *)all + 0xffc);
                            
  // Set the input image
  init_image(nx, ny, width, height, image);

  // Call the stencil kernel
  double tic = wtime();

  stencil(nx, ny, width, height, niters, image);

  double toc = wtime();

  // Output
  printf("------------------------------------\n");
  printf(" runtime: %lf s\n", toc - tic);
  printf("------------------------------------\n");

  output_image(OUTPUT_FILE, nx, ny, width, height, image);
}


void stencil(const size_t nx, const size_t ny, const size_t width, const size_t height, const size_t niters,
             float* image) {

  const size_t border_size              = niters;
  const size_t border_size_padding_bits = 32 - __builtin_clz (border_size-1); // round up to the next power of 2
  const size_t border_size_with_padding = 1 << border_size_padding_bits;

  // TODO
  //const size_t full_cost                = nx * ny * niters;
  //const size_t prec_cost                = (64 * 64 * niters) + (border_size * 64 * niters) + (border_size * 64 * niters / 2);

  if (2 * border_size >= nx || 2 * border_size >= ny) {
    stencil_full(nx, ny, width, height, image, niters);
  } else {
    // ============== PRECOMPUTE STUFF ============================
    float * restrict center = precompute_center(niters);
    struct float_ptr_pair p = precompute_border(niters, 0, false);

    float *upper_border_field = p.ptr1;
    float *left_border_field  = p.ptr2;

    float * restrict upper_left_edge = precompute_symmetric_edge(niters, true);
    float * restrict lower_left_edge;
    float * restrict lower_right_edge;
    float * restrict upper_right_edge;


    // check for symmetries
    if (nx % 64 == 0 && ny % 64 == 0) {
      if (((nx+ny) & 128)) {
        upper_right_edge = upper_left_edge;
        lower_left_edge  = upper_left_edge;
        lower_right_edge = upper_left_edge;
      } else {
        lower_right_edge = upper_left_edge;
        upper_right_edge = precompute_symmetric_edge(niters, false);
        lower_left_edge  = upper_right_edge;
      }
    } else {
      // TODO: check whether some edges are the same
      lower_left_edge  = precompute_full_edge(niters, 0, ~((ny-1)^64) % 128);
      lower_right_edge = precompute_full_edge(niters, ~((nx-1)^64) % 128, ~((ny-1)^64) % 128);
      upper_right_edge = precompute_full_edge(niters, ~((nx-1)^64) % 128, 0);
    }


    if (nx % 64) {
      p = precompute_border(niters, (nx-border_size-1) % 128, true);
    }
    float *right_border_field = p.ptr2;

    float *lower_border_field;
    if (ny % 64 == nx % 64) {
      lower_border_field = p.ptr1;
    } else if(ny % 64) {
      p = precompute_border(niters, (ny-border_size-1) % 128, true);
      lower_border_field = p.ptr1;
    } else {
      lower_border_field = upper_border_field;
    }



    // ============ COPY COMPUTED STUFF INTO FIELD ================
    // TODO: instead of mirroring, mirror it only once
    const size_t tmp = 2 * border_size + 1;
    const size_t edge_width = (tmp + (EDGE_ALIGNMENT/sizeof(float)) - 1) & ~((EDGE_ALIGNMENT/sizeof(float)) - 1);

    // fill center
    const size_t col_start = border_size % 128;
    const size_t fit_start = border_size + (128-col_start);
    const size_t col_end   = (nx-border_size) % 128;
    const size_t fit_end   = nx-border_size - col_end;

    for (size_t row = border_size; row < ny-border_size; row++) {
      float * restrict cur_row_read  = center + ((row & 0x7f) << 7);
      float * restrict cur_row_write = image  + ((row+1) * width + 1);

      float * restrict src = cur_row_read + col_start;
      float * restrict dst = cur_row_write + border_size;
      size_t num = 128-col_start;

      for(size_t i = 0; i < num; i++) {
        dst[i] = src[i];
      }

      src = cur_row_read;
      for(size_t col = fit_start; col < fit_end; col+=128) {
        dst = __builtin_assume_aligned(cur_row_write + col, MAIN_FIELD_ALIGNMENT);
        for (size_t i = 0; i < 128; i++) {
          dst[i] = src[i];
        }
      }

      dst = cur_row_write + fit_end;
      for(size_t i = 0; i < col_end; i++) {
        dst[i] = src[i];
      }
    }
    
    // TODO: vectorize

    // fill upper left edge
    for (size_t row = 0; row < border_size; row++) {
      float * restrict cur_row_read  = upper_left_edge + (row * edge_width);
      float * restrict cur_row_write = image  + ((row+1) * width + 1);

      for(size_t col = 0; col < border_size; col++) {
        cur_row_write[col] = cur_row_read[col];
      }
    }

    // fill upper border
    for (size_t row = 0; row < border_size; row++) {
      float * restrict cur_row_write = image              + ((row+1) * width + 1);
      float * restrict cur_row_read  = upper_border_field + (row << 7);

      for (size_t col = border_size; col < nx-border_size; col++) {
        cur_row_write[col] = cur_row_read[col & 0x7f];
      }
    }

    // fill upper right edge
    for (size_t row = 0; row < border_size; row++) {
      float * restrict cur_row_read  = upper_right_edge + (row * edge_width);
      float * restrict cur_row_write = image  + ((row+1) * width + nx - border_size + 1);

      for(size_t col = 0; col < border_size; col++) {
        cur_row_write[col] = cur_row_read[border_size - col - 1];
      }
    }

    // fill left border
    for (size_t row = border_size; row < ny-border_size; row++) {
      float * restrict cur_row_write = image             + ((row+1) * width + 1);
      float * restrict cur_row_read  = left_border_field + (((row & 0x7f) << border_size_padding_bits));

      for (size_t col = 0; col < border_size; col++) {
        cur_row_write[col] = cur_row_read[col];
      }
    }

    // fill right border
    const int bit_field = border_size_with_padding - 1;
    if (right_border_field == left_border_field) {
      for (size_t row = border_size; row < ny-border_size; row++) {
        float * restrict cur_row_write = image              + ((row+1) * width + 1);
        float * restrict cur_row_read;
        if (ny & 64) {
          cur_row_read  = right_border_field + ((row & 0x7f) << border_size_padding_bits);
        } else {
          cur_row_read  = right_border_field + ((~row & 0x7f) << border_size_padding_bits);
        }

        for (size_t col = nx-border_size; col < nx; col++) {
          cur_row_write[col] = cur_row_read[(border_size - (col - nx + border_size) - 1) & bit_field];
        }
      }
    } else {
      const size_t colstart = nx - border_size;
      for (size_t row = border_size; row < ny-border_size; row++) {
        float * restrict cur_row_write = image              + ((row+1) * width + 1);
        float * restrict cur_row_read  = right_border_field + ((row & 0x7f) << border_size_padding_bits);

        for (size_t col = colstart; col < nx; col++) {
          cur_row_write[col] = cur_row_read[col - colstart];
        }
      }
    }


    // fill lower left edge
    for (size_t row = 0; row < border_size; row++) {
      float * restrict cur_row_read  = lower_left_edge + ((border_size - row - 1) * edge_width);
      float * restrict cur_row_write = image  + (((ny - border_size) + row+1) * width + 1);

      for(size_t col = 0; col < border_size; col++) {
        cur_row_write[col] = cur_row_read[col];
      }
    }

    // fill lower border
    if (lower_border_field == upper_border_field) {
      for (size_t row = ny - border_size; row < ny; row++) {
        float * restrict cur_row_write = image              + ((row+1) * width + 1);
        float * restrict cur_row_read  = lower_border_field + ((ny-row-1) << 7);

        if (lower_border_field == upper_border_field && ny & 64) {
          for (size_t col = border_size; col < nx-border_size; col++) {
            cur_row_write[col] = cur_row_read[col & 0x7f];
          }
        } else {
          for (size_t col = border_size; col < nx-border_size; col++) {
            cur_row_write[col] = cur_row_read[(~col) & 0x7f];
          }
        }
      }
    } else {
      const size_t rowstart = ny - border_size;
      for (size_t row = rowstart; row < ny; row++) {
        float * restrict cur_row_write = image              + ((row+1) * width + 1);
        float * restrict cur_row_read  = lower_border_field + ((row-rowstart) << 7);

        if (nx % 128 == ny % 128 || nx % 64 != ny %64) {
          for (size_t col = border_size; col < nx-border_size; col++) {
            cur_row_write[col] = cur_row_read[col & 0x7f];
          }
        } else {
          for (size_t col = border_size; col < nx-border_size; col++) {
            cur_row_write[col] = cur_row_read[~col & 0x7f];
          }
        }
      }
    }

    // fill lower right edge
    for (size_t row = 0; row < border_size; row++) {
      float * restrict cur_row_read  = lower_right_edge + ((border_size - row - 1) * edge_width);
      float * restrict cur_row_write = image  + (((ny-border_size) + row+1) * width + nx - border_size + 1);

      for(size_t col = 0; col < border_size; col++) {
        cur_row_write[col] = cur_row_read[border_size - col - 1];
      }
    }

  }
}




// this function just stencils the whole field, assuming black borders around it
void stencil_full(const size_t nx, const size_t ny, const size_t width, const size_t height, float* image, const size_t niters)
{
  float tmp;

  void *all_ptr = NULL;
  const size_t aligned = (nx + 31) & ~31;
  if (posix_memalign(&all_ptr, 32, aligned*3*sizeof(float))) {
    fprintf(stderr, "Memory Error\n");
    exit(-1);
  }
  float * restrict row_buffer1 = __builtin_assume_aligned((float *)all_ptr, 32);
  float * restrict row_buffer2 = __builtin_assume_aligned(row_buffer1 + aligned, 32);
  float * restrict row_buffer3 = __builtin_assume_aligned(row_buffer2 + aligned, 32);

  float * restrict prev_row_bak, * restrict cur_row_bak, * restrict tmp2;

  prev_row_bak = row_buffer1; // holds the original previous row
  cur_row_bak = row_buffer2;  // holds the original current row

  // start stencilin'
  for (size_t i = 0; i < niters; i++) {

    memset(prev_row_bak, 0, nx * sizeof(float)); // first 'invisible' row is black

    for (size_t row = 0; row < ny; row++) {
      float * restrict cur_row = image + ((row+1) * width) + 1;

      // stencil over the horizontal
      row_buffer3[ 0] = 0.0f + cur_row[ 0] * 6.0f + cur_row[ 1];
      for (size_t col = 1; col < nx-1; col++) {
        row_buffer3[col] = cur_row[col] * 6.0f + cur_row[col+1] + cur_row[col-1];
      }
      row_buffer3[nx-1] = 0.0f + cur_row[nx-1] * 6.0f + cur_row[nx-2];
      
      for (size_t i = 0; i < nx; i++) {
        cur_row_bak[i] = cur_row[i];
      }

      // row additions
      // Add previous and following line
      float * restrict nxt_row = cur_row + width;
      for (size_t col = 0; col < nx; col++) {
        tmp = prev_row_bak[col] + nxt_row[col] + row_buffer3[col];
        cur_row[col] = tmp * 0.1f;
      }

      // switch buffers
      tmp2 = prev_row_bak;
      prev_row_bak = cur_row_bak;
      cur_row_bak = tmp2;
    }
  }

  free(all_ptr);
}


// Create the input image
void init_image(const int nx, const int ny, const int width, const int height,
                float *image)
{
  const int tile_size = 64;
  // checkerboard pattern
  for (int yb = 0; yb < ny; yb += tile_size) {
    for (int xb = 0; xb < nx; xb += tile_size) {
      if ((xb + yb) % (tile_size * 2)) {
        const int ylim = (yb + tile_size > ny) ? ny : yb + tile_size;
        const int xlim = (xb + tile_size > nx) ? nx : xb + tile_size;
        for (int y = yb + 1; y < ylim + 1; y++) {
          for (int x = xb + 1; x < xlim + 1; x++) {
            image[x + y * width] = WHITE_FLOAT;
          }
        }
      }
    }
  }
}



// Routine to output the image in Netpbm grayscale binary image format
void output_image(const char* file_name, const int nx, const int ny,
                  const int width, const int height, float* image)
{
  // Open output file
  FILE* fp = fopen(file_name, "w");
  if (!fp) {
    fprintf(stderr, "Error: Could not open %s\n", OUTPUT_FILE);
    exit(EXIT_FAILURE);
  }

  // Ouptut image header
  fprintf(fp, "P5 %d %d 255\n", nx, ny);

  // Calculate maximum value of image
  // This is used to rescale the values
  // to a range of 0-255 for output
  float maximum = 0.0;
  for (int y = 1; y < ny + 1; y++) {
    for (int x = 1; x < nx + 1; x++) {
      if (image[x + y * width] > maximum) maximum = image[x + y * width];
    }
  }

  // Output image, converting to numbers 0-255
  for (int y = 1; y < ny + 1; y++) {
    for (int x = 1; x < nx + 1; x++) {
      fputc((char)(255.0f * image[x + y * width] / maximum), fp);
    }
  }

  // Close the file
  fclose(fp);
}





// =====================================================================================================
// NOT TOUCHED
// =====================================================================================================


// Get the current time in seconds since the Epoch
double wtime(void)
{
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return tv.tv_sec + tv.tv_usec * 1e-6;
}

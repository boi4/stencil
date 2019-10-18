#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/time.h>
#include <sys/mman.h>

#include "stencil.h"


void stencil(const size_t nx, const size_t ny, const size_t width, const size_t height, const size_t niters, float* image);
void stencil_full(const size_t nx, const size_t ny, float* image, size_t niters);
void init_image(const int nx, const int ny, const int width, const int height, float* image);
void output_image(const char* file_name, const int nx, const int ny,
                  const int width, const int height, float* image);
double wtime(void);

// all externs are defined in stencil_precomupte.c
extern float *precompute_center(size_t niters);
extern struct float_ptr_pair precompute_upper_border(size_t niters);
extern struct float_ptr_pair precompute_lower_border(const size_t niters, const size_t length);

// debug stuff
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
  size_t nx     = strtoul(argv[1], NULL, 0);
  size_t ny     = strtoul(argv[2], NULL, 0);
  size_t niters = 2*strtoul(argv[3], NULL, 0);

  // we pad the outer edge of the image to avoid out of range address issues in
  // stencil
  size_t width = nx + 2;
  size_t height = ny + 2;

  // Allocate the image
  size_t image_size = ((width * height * sizeof(float)) + 0x1000) & (~(size_t)0xfff);

  float * restrict image = (float *)mmap(NULL, image_size,
                               PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS,
                               -1, 0);

  if (!image) {
    fprintf(stderr, "Memory Error\n");
    exit(-1);
  }
                            
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

  const size_t border_size = niters;
  const size_t border_size_padding_bits = 32 - __builtin_clz (border_size-1); // round up to the next power of 2
  const size_t border_size_with_padding = 1 << border_size_padding_bits;

  if (nx < 2 * border_size || ny < 2 * border_size) { // we cannot optimize in this case
    stencil_full(nx, ny, image, niters);
  } else {
    //stencil_full(nx, ny, image, niters);
    // precompute stuff
    struct float_ptr_pair p;

    float * restrict center = precompute_center(niters);

    p = precompute_upper_border(niters);

    float *upper_border_field = p.ptr1;
    float *left_border_field = p.ptr2;


    // maybe we need to do some more calculations
    if (nx % 64) {
      p = precompute_lower_border(niters, nx);
    }
    float *right_border_field = p.ptr2;
    if (ny % 64 && ny != nx) {
      p = precompute_lower_border(niters, ny);
    }
    float *lower_border_field = p.ptr1;


    // fill center
    for (size_t row = border_size; row < ny-border_size; row++) {
      float * restrict cur_row_read = center + ((row & 0x7f) << 7);
      float * restrict cur_row_write = &image[(row+1) * width+1];

      for(size_t col = border_size; col < nx-border_size; col++) {
        cur_row_write[col] = cur_row_read[col & 0x7f];
      }
    }

    // fill upper border
    for (size_t row = 0; row < border_size; row++) {
      float * restrict cur_row_write = &image[(row+1) * width + 1];
      float * restrict cur_row_read = &upper_border_field[row << 7];
      for (size_t col = border_size; col < nx-border_size; col++) {
        cur_row_write[col] = cur_row_read[col % 128];
      }
    }

    // fill left border
    for (size_t row = border_size; row < ny-border_size; row++) {
      float * restrict cur_row_write = image + ((row+1) * width + 1);
      float * restrict cur_row_read = left_border_field + (((row % 128) << border_size_padding_bits));
      for (size_t col = 0; col < border_size; col++) {
        cur_row_write[col] = cur_row_read[col];
      }
    }

    // TODO add new vars instead of the whole col - (...) thing
    // fill lower border
    if (lower_border_field == upper_border_field) {
      for (size_t row = ny - border_size - 0; row < ny; row++) {
        float * restrict cur_row_write = &image[(row+1) * width + 1];
        float * restrict cur_row_read = &lower_border_field[(ny-row-1) << 7];
        for (size_t col = border_size; col < nx-border_size; col++) {
          cur_row_write[col] = cur_row_read[(127-col) % 128];
        }
      }
    } else { // TODO
      for (size_t row = ny - border_size - 1; row < ny; row++) {
        float * restrict cur_row_write = &image[(row+1) * width + 1];
        float * restrict cur_row_read = &lower_border_field[(row-(ny-border_size-1)) << 7];
        for (size_t col = border_size; col < nx-border_size; col++) {
          cur_row_write[col] = cur_row_read[col % 128];
        }
      }
    }

    // fill right border
    const int bit_field = border_size_with_padding - 1;
    if (right_border_field == left_border_field) {
      for (size_t row = border_size; row < ny-border_size; row++) {
        float * restrict cur_row_write = image + ((row+1) * width + 1);
        float * restrict cur_row_read = right_border_field + (((127-row) % 128) << border_size_padding_bits);
        for (size_t col = nx-border_size; col < nx; col++) {
          cur_row_write[col] = cur_row_read[(border_size - (col - nx + border_size) - 1) & bit_field];
        }
      }
    } else { // TODO
      for (size_t row = border_size; row < ny-border_size; row++) {
        float * restrict cur_row_write = &image[(row+1) * width + 1];
        float * restrict cur_row_read = &right_border_field[(row % 128) << border_size_padding_bits];
        for (size_t col = nx-border_size-1; col < nx; col++) {
          cur_row_write[col] = cur_row_read[col - (nx-border_size-1)];
        }
      }
    }

  }
}




// this function just stencils the whole field, assuming black borders around it
void stencil_full(const size_t nx, const size_t ny, float* image, const size_t niters)
{
  size_t width  = nx + 2;
  float tmp;

  void *all_ptr = NULL;
  if (posix_memalign(&all_ptr, 0x10, nx*3*sizeof(float))) {
    fprintf(stderr, "Memory Error\n");
    exit(-1);
  }
  float * restrict row_buffer1 = (float *)all_ptr;
  float * restrict row_buffer2 = row_buffer1 + nx;
  float * restrict row_buffer3 = row_buffer2 + nx;

  float * restrict prev_row_bak, * restrict cur_row_bak, * restrict tmp2;

  prev_row_bak = row_buffer1; // holds the original previous row
  cur_row_bak = row_buffer2;  // holds the original current row

  // start stencilin'
  for (size_t i = 0; i < niters; i++) {

    prev_row_bak = image;

    for (size_t row = 0; row < ny; row++) {
      float * restrict cur_row = image + ((row+1) * width) + 1;

      // stencil over the horizontal
      row_buffer3[ 0] = 0.0f + cur_row[ 0] * 6.0f + cur_row[ 1];
      for (size_t col = 1; col < nx-1; col++) {
        row_buffer3[col] = cur_row[col] * 6.0f + cur_row[col+1] + cur_row[col-1];
      }
      row_buffer3[nx-1] = 0.0f + cur_row[nx-1] * 6.0f + cur_row[nx-2];
      
      memcpy(cur_row_bak, cur_row, nx * sizeof(float));

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

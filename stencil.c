#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/time.h>
#include <sys/mman.h>

#include "stencil.h"


void stencil(const int nx, const int ny, const int width, const int height, const int niters,
             float* image, float* tmp_image);
void stencil_full(const int nx, const int ny, const int width, const int height,
             float* image, float* tmp_image);
void init_image(const int nx, const int ny, const int width, const int height,
                float* image, float* tmp_image);
void output_image(const char* file_name, const int nx, const int ny,
                  const int width, const int height, float* image);

struct float_ptr_pair {
  float *ptr1;
  float *ptr2;
};



// all externs are defined in stencil_precomupte.c
extern void precompute_center(size_t niters);
extern struct float_ptr_pair precompute_upper_border(size_t niters);
extern struct float_ptr_pair precompute_lower_border(const size_t niters, const size_t length);

extern void dump_field(char *fname, int nx, int ny, float *field);
extern void print_row(float *row, size_t length);

extern float center_small[64][64];
extern float center_big[128][128];

double wtime(void);



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
  int width = nx + 2;
  int height = ny + 2;

  // Allocate the image
  unsigned long image_size = ((width * height * sizeof(float)) + 0x1000) & (~(unsigned long)0xfff);

  float * restrict image = (float *)mmap(NULL, image_size * 2,
                               PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS,
                               -1, 0);
  float * restrict tmp_image = (float *)((char *)image + image_size);

  if (!image) {
    fprintf(stderr, "Memory Error\n");
    exit(-1);
  }
                            
  // Set the input image
  init_image(nx, ny, width, height, image, tmp_image);

  // Call the stencil kernel
  double tic = wtime();

  stencil(nx, ny, width, height, niters, image, tmp_image);

  double toc = wtime();

  // Output
  printf("------------------------------------\n");
  printf(" runtime: %lf s\n", toc - tic);
  printf("------------------------------------\n");

  //output_image(OUTPUT_FILE, nx, ny, width, height, image);
}


void stencil(const int nx, const int ny, const int width, const int height, const int niters,
             float* image, float* tmp_image) {

  const size_t border_size = niters;
  const size_t border_size_padding_bits = 32 - __builtin_clz (border_size-1); // round up to the next power of 2
  const size_t border_size_with_padding = 1 << border_size_padding_bits;

  if (nx < 2 * border_size || ny < 2 * border_size) { // we cannot optimize in this case
    for (int t = 0; t < niters/2; ++t) { // TODO: find different solution
      stencil_full(nx, ny, width, height, image, tmp_image);
      stencil_full(nx, ny, width, height, tmp_image, image);
    }
  } else {
//    for (int t = 0; t < niters/2; ++t) { // TODO: find different solution
//      stencil_full(nx, ny, width, height, image, tmp_image);
//      stencil_full(nx, ny, width, height, tmp_image, image);
//    }
    // precompute stuff
    struct float_ptr_pair p;

    // result will be stored in center_big matrix
    precompute_center(niters);

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
      float * restrict cur_row_read = center_big[row & 0x7f];
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




void stencil_full(const int nx, const int ny, const int width, const int height,
             float* image, float* tmp_image)
{
  float sum, cur, nxt;

  size_t lim = (ny + 1) * width;
  for (size_t offset = width; offset < lim; offset += width) { // offset is row * width
    cur = image[offset];
    nxt = image[offset + 1];
    for (size_t col = 1; col < nx + 1; col++) {
      sum  = cur; // add previous field
      sum += nxt * 6.0f; // add current field
      cur  = nxt;
      nxt  = image[offset+(col + 1)]; // next field
      sum += nxt; // add next field
      sum += image[(offset+col) - width]; // field above
      sum += image[(offset+col) + width]; // field below
      tmp_image[offset + col] = sum * 0.1f;
    }
  }
}


// Create the input image
void init_image(const int nx, const int ny, const int width, const int height,
                float *image, float *tmp_image)
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

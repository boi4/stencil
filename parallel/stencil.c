#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/time.h>
#include <sys/mman.h>

#include <mpi.h>

// Define output file name
#define OUTPUT_FILE "stencil.pgm"
#define WHITE_FLOAT 100.0f
#define BORDER_FIELD_ALIGNMENT 0x80 // should be at least 32 for vectorization and not greater than 0x1000
#define CORNER_ALIGNMENT 64
#define MAIN_FIELD_ALIGNMENT 64

#define MIN(a,b) (((a)<(b))?(a):(b))


struct float_ptr_pair {
  float * ptr1;
  float * ptr2;
};

struct dimensions {
  size_t nx;
  size_t ny;
  size_t width;
  size_t height;
};


void stencil(struct dimensions const d, float* image, float* tmp_image);

void stencil_full(struct dimensions const d, float* image, float *tmp_image, size_t niters);

void stencil_full_inplace(struct dimensions const d, float* image, size_t niters);

void init_image(struct dimensions const d, float* image);

void output_image(const char* file_name, struct dimensions const d, float* image);

double wtime(void);






int nprocs, rank;


void master(struct dimensions const d, size_t niters) {
  // Set the input image
  //init_image(d, image);

  // Call the stencil kernel
  double tic = wtime();

  //stencil_full(d, image, tmp_image, niters);
  //stencil_full_inplace(nx, ny, width, height, image, niters);

  double toc = wtime();

  // Output
  printf("------------------------------------\n");
  printf(" runtime: %lf s\n", toc - tic);
  printf("------------------------------------\n");

  //output_image(OUTPUT_FILE, d, image);
}



void worker(struct dimensions const d, const size_t niters) {

}



int main(int argc, char* argv[])
{
  // Check usage
  if (argc != 4) {
    fprintf(stderr, "Usage: %s nx ny niters\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  MPI_Init(&argc, &argv);
  MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);

  // Initiliase problem dimensions from command line arguments
  size_t nx     =   strtoul(argv[1], NULL, 0);
  size_t ny     =   strtoul(argv[2], NULL, 0);
  size_t niters = 2*strtoul(argv[3], NULL, 0);

  // we pad width, so each row is 128 bit aligned
  // stencil
  size_t width  = ((nx + 2) + MAIN_FIELD_ALIGNMENT - 1) & ~(MAIN_FIELD_ALIGNMENT - 1);
  size_t height = ny + 2;

  //printf("%d %d\n", nprocs, rank);
  switch(rank) {
    case 0:
      master((struct dimensions){nx, ny, width, height}, niters);
      break;

    default:
      worker((struct dimensions){nx, ny, width, height}, niters);
      break;
  }
}





void stencil(struct dimensions const d, float* image, float* tmp_image) {
  for (int i = 1; i < d.nx + 1; ++i) {
    for (int j = 1; j < d.ny + 1; ++j) {
      float tmp;
      tmp  = image[j     + i       * d.height] * 6.0f;
      tmp += image[j     + (i - 1) * d.height];
      tmp += image[j     + (i + 1) * d.height];
      tmp += image[j - 1 + i       * d.height];
      tmp += image[j + 1 + i       * d.height];
      tmp_image[j + i * d.height] = tmp *0.1f;
    }
  }
}



void init_rows(struct dimensions const d, float *image, size_t start_row, size_t nrows) {

}



/* this function just stencils the whole field, assuming black borders around it
 * only used if precomputing isn't faster
 */
void stencil_full_inplace(struct dimensions const d, float* image, const size_t niters)
{
  float tmp;

  void *all_ptr = NULL;
  const size_t aligned = (d.nx + 63) & ~63;
  if (posix_memalign(&all_ptr, 64, aligned*3*sizeof(float))) {
    fprintf(stderr, "Memory Error\n");
    exit(-1);
  }
  float * restrict row_buffer1 = __builtin_assume_aligned((float *)all_ptr, 64);
  float * restrict row_buffer2 = __builtin_assume_aligned(row_buffer1 + aligned, 64);
  float * restrict row_buffer3 = __builtin_assume_aligned(row_buffer2 + aligned, 64);

  float * restrict prev_row_bak, * restrict cur_row_bak, * restrict tmp2;

  prev_row_bak = row_buffer1; // holds the original previous row
  cur_row_bak = row_buffer2;  // holds the original current row

  // start stencilin'
  for (size_t i = 0; i < niters; i++) {

    memset(prev_row_bak, 0, d.nx * sizeof(float)); // first 'invisible' row is black

    for (size_t row = 0; row < d.ny; row++) {
      float * restrict cur_row = image + ((row+1) * d.width) + 1;

      // stencil over the horizontal
      row_buffer3[ 0] = 0.0f + cur_row[ 0] * 6.0f + cur_row[ 1];
      for (size_t col = 1; col < d.nx-1; col++) {
        row_buffer3[col] = cur_row[col] * 6.0f + cur_row[col+1] + cur_row[col-1];
      }
      row_buffer3[d.nx-1] = 0.0f + cur_row[d.nx-1] * 6.0f + cur_row[d.nx-2];
      
      for (size_t i = 0; i < d.nx; i++) {
        cur_row_bak[i] = cur_row[i];
      }

      // row additions
      // Add previous and following line
      float * restrict nxt_row = cur_row + d.width;
      for (size_t col = 0; col < d.nx; col++) {
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





void stencil_full(struct dimensions const d, float* image, float *tmp_image, const size_t niters) {
  for (int t = 0; t < niters/2; ++t) {
    stencil(d, image, tmp_image);
    stencil(d, tmp_image, image);
  }
}


// Create the input image
void init_image(struct dimensions const d, float *image)
{
  const int tile_size = 64;
  // checkerboard pattern
  for (int yb = 0; yb < d.ny; yb += tile_size) {
    for (int xb = 0; xb < d.nx; xb += tile_size) {
      if ((xb + yb) % (tile_size * 2)) {
        const int ylim = (yb + tile_size > d.ny) ? d.ny : yb + tile_size;
        const int xlim = (xb + tile_size > d.nx) ? d.nx : xb + tile_size;
        for (int y = yb + 1; y < ylim + 1; y++) {
          for (int x = xb + 1; x < xlim + 1; x++) {
            image[x + y * d.width] = WHITE_FLOAT;
          }
        }
      }
    }
  }
}



// Routine to output the image in Netpbm grayscale binary image format
void output_image(const char* file_name, struct dimensions const d, float* image)
{
  // Open output file
  FILE* fp = fopen(file_name, "w");
  if (!fp) {
    fprintf(stderr, "Error: Could not open %s\n", OUTPUT_FILE);
    exit(EXIT_FAILURE);
  }

  // Ouptut image header
  fprintf(fp, "P5 %d %d 255\n", d.nx, d.ny);

  // Calculate maximum value of image
  // This is used to rescale the values
  // to a range of 0-255 for output
  float maximum = 0.0;
  for (int y = 1; y < d.ny + 1; y++) {
    for (int x = 1; x < d.nx + 1; x++) {
      if (image[x + y * d.width] > maximum) maximum = image[x + y * d.width];
    }
  }

  // Output image, converting to numbers 0-255
  for (int y = 1; y < d.ny + 1; y++) {
    for (int x = 1; x < d.nx + 1; x++) {
      fputc((char)(255.0f * image[x + y * d.width] / maximum), fp);
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

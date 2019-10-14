#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/mman.h>


// Define output file name
#define OUTPUT_FILE "stencil.pgm"


void stencil(const int nx, const int ny, const int width, const int height,
             float* image, float* tmp_image);
void init_image(const int nx, const int ny, const int width, const int height,
                float* image, float* tmp_image);
void output_image(const char* file_name, const int nx, const int ny,
                  const int width, const int height, float* image);

void stencil_four_tiles(size_t niters);

double wtime(void);

int main(int argc, char* argv[])
{
  /////// Check usage
  /////if (argc != 4) {
  /////  fprintf(stderr, "Usage: %s nx ny niters\n", argv[0]);
  /////  exit(EXIT_FAILURE);
  /////}

  /////// Initiliase problem dimensions from command line arguments
  /////// TODO: change to unsigned, dont't use atoi
  /////int nx = atoi(argv[1]);
  /////int ny = atoi(argv[2]);
  /////int niters = atoi(argv[3]);

  /////// we pad the outer edge of the image to avoid out of range address issues in
  /////// stencil
  /////int width = nx + 2;
  /////int height = ny + 2;

  /////// Allocate the image
  /////unsigned long image_size = ((width * height * sizeof(float)) + 0x1000) & (~(unsigned long)0xfff);

  /////float *image = (float *)mmap(NULL, image_size * 2,
  /////                             PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS,
  /////                             -1, 0);
  /////float *tmp_image = (float *)((char *)image + image_size);
  /////                          
  /////                         
  /////// check alignment
  ///////printf("image at %p\n", image);
  ///////printf("tmp_image at %p\n", tmp_image);

  /////// Set the input image
  /////init_image(nx, ny, width, height, image, tmp_image);

  // Call the stencil kernel
  double tic = wtime();


  /////for (int t = 0; t < niters; ++t) {
  /////  stencil(nx, ny, width, height, image, tmp_image);
  /////  stencil(nx, ny, width, height, tmp_image, image);
  /////}

  stencil_four_tiles(atoi(argv[1]));

  double toc = wtime();

  // Output
  printf("------------------------------------\n");
  printf(" runtime: %lf s\n", toc - tic);
  printf("------------------------------------\n");

  //output_image(OUTPUT_FILE, nx, ny, width, height, image);
}


float field[64][64]; // [row][column], compiler will make two elements on same row be next to each other
float row_buffer[64]; // holds the previous row


/* This function will stenctil the part where four tiles meet
 * it will simulate an infinite space by wrapping around the corners
 */
void stencil_four_tiles(size_t niters) {
  // create chessboard pattern
  for (size_t i = 0; i < 32; i++) {
    row_buffer[i] = 100.0;
  }
  for (size_t i = 32; i < 64; i++) {
    row_buffer[i] = 0.0;
  }
  for (size_t i = 0; i < 32; i++) {
      memcpy(field[i], row_buffer, sizeof(row_buffer));
  }
  for (size_t i = 0; i < 32; i++) {
    row_buffer[i] = 0.0;
  }
  for (size_t i = 32; i < 64; i++) {
    row_buffer[i] = 100.0;
  }
  for (size_t i = 32; i < 64; i++) {
      memcpy(field[i], row_buffer, sizeof(row_buffer));
  }

  // all fields
  float sum, cur, nxt, tmp;

  // switch this one around
  for(size_t i = 0; i < 64; i++) {row_buffer[i] = field[63][63-i];}

  for (size_t i = 0; i < niters; i++) {
    for (size_t row = 0; row < 63; row++) {
      cur = field[63-row][63];
      nxt = field[row][0];
      for(size_t col = 0; col < 63; col++) {
        sum  = cur; // add previous field
        sum += nxt * 6.0f; // add current field
        cur  = nxt;
        nxt  = field[row][col+1];
        sum += nxt; // add next field
        field[row][col] = sum / 8.0f;
      }
      // special case to avoid some modulo
      sum  = cur; // add previous field
      sum += nxt * 6.0f; // add current field
      cur  = nxt;
      nxt  = field[63-row][0];
      sum += nxt; // add next field
      field[row][63] = sum * 0.1;
      // TODO: vecorize
      // Add previous and following ling
      float *nxt_row = field[row + 1];
      for (size_t col = 0; col < 64; col++) {
        tmp = row_buffer[col] + nxt_row[col];
        field[row][col] += tmp * 0.1;
      }

      memcpy(row_buffer, field[row], sizeof(row_buffer));
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
      field[63][col] = sum / 8.0f;
    }
    // special case to avoid some modulo
    sum  = cur; // add previous field
    sum += nxt * 6.0f; // add current field
    cur  = nxt;
    nxt  = field[0][0];
    sum += nxt; // add next field
    field[63][63] = sum * 0.1;
    // TODO: vecorize
    // Add previous and following ling
    float *nxt_row = field[0];
    for (size_t col = 0; col < 64; col++) {
      tmp = row_buffer[col] + nxt_row[63-col];
      field[63][col] += tmp * 0.1;
    }

    for(size_t i = 0; i < 64; i++) {row_buffer[i] = field[63][63-i];}
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



void stencil(const int nx, const int ny, const int width, const int height,
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
//void stencil(const int nx, const int ny, const int width, const int height,
//             double* image, double* tmp_image)
//{
//  for (int j = 1; j < ny + 1; ++j) {
//    for (int i = 1; i < nx + 1; ++i) {
//      tmp_image[j + i * height] =  image[j     + i       * height] * 3.0 / 5.0;
//      tmp_image[j + i * height] += image[j     + (i - 1) * height] * 0.5 / 5.0;
//      tmp_image[j + i * height] += image[j     + (i + 1) * height] * 0.5 / 5.0;
//      tmp_image[j + i * height] += image[j - 1 + i       * height] * 0.5 / 5.0;
//      tmp_image[j + i * height] += image[j + 1 + i       * height] * 0.5 / 5.0;
//    }
//  }
//}

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
            image[x + y * width] = 100.0;
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

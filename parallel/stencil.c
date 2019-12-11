#include "stencil.h"

#include "util.c"

// assemble the whole image by taking all results
void assemble_image(struct dimensions d, float *image
                    , struct result *results, size_t maxfieldsize) {
  for (int i = 0; i < nprocs; i++) {
    struct result *r = (struct result *) ((uint8_t *)results + 
                  (i * (maxfieldsize*sizeof(float) + sizeof(struct result))));
    /* print_config(r->c); */
    //if (i == -1) {
    if (false) {
      output_image("test.pgm", (struct dimensions){
                                                     .nx = r->c.width,
                                                     .ny = r->c.height,
                                                     .width = r->c.width,
                                                     .height = r->c.height
                                                     }, (float *)r->field);
    }
    for (int y = r->c.y; y < r->c.y+r->c.height; y++) {
      for (int x = r->c.x; x < r->c.x+r->c.width; x++) {
        image[y * d.width + x] = r->field[r->c.width*(y-r->c.y) + (x-r->c.x)];
      }
    }

  }
}

//============ write file ====================
void share_results(struct result const * const res, size_t maxfieldsize,
                   struct dimensions fulld) {
  float *image;
  struct result *results;

  if (rank == 0) {
    image = calloc(fulld.height * fulld.width, sizeof(float));
    results = calloc(nprocs, sizeof(struct result) + maxfieldsize*sizeof(float));
    if (!results || !image) {
      fprintf(stderr, "Memory Error on line %zu\n", __LINE__);
      exit(-1);
    }
  }

  /* printf("sizeof block: %zu\n",sizeof(struct result)+maxfieldsize*sizeof(float)); */
  MPI_Gather(res,sizeof(struct result)+maxfieldsize*sizeof(float), MPI_CHAR,
             results,sizeof(struct result)+maxfieldsize*sizeof(float), MPI_CHAR,
             0, MPI_COMM_WORLD);

  if (rank == 0) {
    assemble_image(fulld, image, results, maxfieldsize);
    output_image(OUTPUT_FILE, fulld, image);
    free(image);
    free(results);
  }
}

// we will allocate a two row/two column halo region
// so we can alternatingly swap horizontally and vertically
void prepare_images(struct config const * const c, float *images[static 2]) {
  size_t const image_size = (c->width + 2*2) * (c->height + 2*2);
  images[0] = (float *) calloc(image_size, sizeof(float));
  images[1] = (float *) calloc(image_size, sizeof(float));
  if (!images[0] || !images[1]) {
    fprintf(stderr, "Image Allocation failed. Please restart application\n");
    exit(-1);
  }

  // setup chessboard pattern on first image
  // row-major
  for (size_t row = c->y; row < c->y+c->height; row++) {
    float * const restrict cur_row = images[0]+(row-c->y+2)*(c->width+2*2)+2;

    for (size_t col = c->x; col < c->x+c->width; col++) {
      cur_row[col-c->x] = ((col & 64) ^ (row & 64)) ? WHITE_FLOAT : 0.0f;
    }
  }
  /* if (rank == -1) { */
  if (false) {
    output_image("test.pgm", (struct dimensions){
                                                   .nx = c->width,
                                                   .ny = c->height,
                                                   .width = c->width + 4,
                                                   .height = c->height + 4
                                                   }, images[0] + (2 * (c->width+4) + 2));
  }
}


// build result for sharing with process 0
// removes margin from image
struct result *buildresult(struct config const * const c, float *field, size_t maxfieldsize) {
  struct result *res = (struct result *)
    calloc(1, sizeof(struct result) + maxfieldsize*sizeof(float));
  if (!res) {
    fprintf(stderr, "Result allocation failed. Please restart application\n");
    exit(-1);
  }
  res->c = *c;
  for (int y = 0; y < c->height; y++) {
    float * restrict cur_row_read = field + (y+2)*(c->width + 2*2) + 2;
    float * restrict cur_row_write = res->field + y*c->width;

    for (int x = 0; x < c->width; x++) {
      cur_row_write[x] = cur_row_read[x];
    }
  }
  return res;
}


void swap_halo(struct config const * const myconf, float * const image,
               bool const left_right_swap) {
  
}



/*
  Main function for a process
  prepares part, computes position, stencils & communicates
  process 0 will measure time and output image to file
 */
void worker(struct dimensions const d, size_t const niters) {
  double tictoc[2];
  float *images[2] = {0};
  float *dst_image;// will be one of the two images

  struct config const myconf = compute_config(d);

  prepare_images(&myconf, images);
  /* printf("%p %p\n", images[0], images[1]); */

  tictoc[0] = wtime();

  /* if (0) */
  dst_image = stencil_and_swap(&myconf, images, niters);

  tictoc[1] = wtime();

  /* if (rank == -1) { */
  if (false) {
    output_image("test.pgm", (struct dimensions){
                                                   .nx = myconf.width,
                                                   .ny = myconf.height,
                                                   .width = myconf.width + 4,
                                                   .height = myconf.height + 4
                                                   }, images[0] + (2 * (myconf.width+4) + 2));
  }

  share_times(tictoc);

  size_t maxfieldsize = MAX(d.nx, d.ny)
    / (size_t)(sqrt(nprocs+4)+0.00001 - 1) + 1;
  maxfieldsize *= maxfieldsize;
  struct result const * const res = buildresult(&myconf, dst_image, maxfieldsize);
  if (rank == -1) {
    output_image("test.pgm", (struct dimensions){
        .nx = res->c.width,
        .ny = res->c.height,
        .width = res->c.width,
        .height = res->c.height
                                                   }, (float *)res->field);
  }
  share_results(res, maxfieldsize, d);

  free((void *)res);

  free(images[0]);
  free(images[1]);
}




void stencil(struct dimensions const d, float* image, float* tmp_image) {
  /* printf("%p %p\n", image, tmp_image); */
  for (int y = 1; y < d.ny + 1; y++) {
    for (int x = 1; x < d.nx + 1; x++) {
      float tmp;
      tmp  = image[  y     *d.width +  x      ] * 6.0f;
      tmp += image[  y     *d.width + (x - 1) ];
      tmp += image[  y     *d.width + (x + 1) ];
      tmp += image[ (y - 1)*d.width +  x      ];
      tmp += image[ (y + 1)*d.width +  x      ];
      tmp_image[y * d.width + x] = tmp * 0.1f;
    }
  }
}



int main(int argc, char* argv[argc+1])
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
  //size_t width  = ((nx + 2) + MAIN_FIELD_ALIGNMENT - 1) & ~(MAIN_FIELD_ALIGNMENT - 1);
  size_t width = nx + 2;
  size_t height = ny + 2;


  worker((struct dimensions){nx, ny, width, height}, niters);
  MPI_Finalize();
}

#ifndef __STENCIL_H
#define __STENCIL_H



// Define output file name
#define OUTPUT_FILE "stencil.pgm"
#define WHITE_FLOAT 100.0f
#define BORDER_FIELD_ALIGNMENT 0x80 // should be at least 32 for vectorization and not greater than 0x1000
#define CORNER_ALIGNMENT 32
#define MAIN_FIELD_ALIGNMENT 32

#define MIN(a,b) (((a)<(b))?(a):(b))


struct float_ptr_pair {
  float * ptr1;
  float * ptr2;
};


#endif

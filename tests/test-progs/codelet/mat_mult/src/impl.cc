#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../codelet.hh"
#include "impl.hh"
#include <sys/mman.h>

#define CODELET_NUM 6

#define TILES 10
//#define TILE_DIM 16
#define REG_SIZE (TILE_DIM*TILE_DIM*sizeof(double))
#define B_offset (REG_SIZE * TILES)
#define C_offset (REG_SIZE * TILES * 2)
#define Test_offset (REG_SIZE * TILES * 2 + REG_SIZE)
#define NumElements_AB ((REG_SIZE*TILES)/sizeof(double))
#define NumElements_C ((REG_SIZE)/sizeof(double))

// this is a statically allocated codelet graph that should be loaded into SU
// dependencies are based on the scm program and managed by the SU
// the last codelet in the list is a dummy codelet with no i/o that 
// holds in its "fire" field the base pointer of SCM memory
user_codelet_t codelet_graph[CODELET_NUM] __attribute__ ((section(".codelet_program"))) = {{OP1_WR | OP2_RD | OP3_RD, "LoadSqTile_2048L", loadSqTile},
                                                                                           {OP1_WR | OP1_RD | OP2_RD | OP3_RD, "MatMult_2048L", matMult},
                                                                                           {OP1_RD | OP2_RD | OP3_RD, "StoreSqTile_2048L", storeSqTile},
                                                                                           {OP1_RD, "InitCod_64B", (fire_t)scm_init}, //for single tile version
                                                                                           {OP1_RD, "InitCod1280_64B", (fire_t)scm_init1280}, //for 128x1280 version
                                                                                           {0, "ScmMemBasePtr", (fire_t)SCM_MEMORY_BASE_PTR} // dummy codelet for scm memory base ptr
                                                                                           };


void initMatrix (double * mat, int elements, int val = 0) {
  for (int i = 0; i < elements; i++)
    mat[i] = (i)*val;
} 

void scm_init() {
    // this is used to offer the users a way to do setup for a benchmark and initialize
    // the memory space that memory codelets will load from
    printf("initializing data space starting at %p\n", SCM_MEMORY_BASE_PTR);
    double * mem_space = (double *) SCM_MEMORY_BASE_PTR;
    // initialize the data space with two matrices TILE_DIM x TILE_DIM matrices
    for (int i=0; i<TILE_DIM*TILE_DIM*2; i++) {
      mem_space[i] = (double) i;
    }
    // doing a matrix multiplication in the 4th matrix slot of the mem space to for error checking at the end
    //printf("result matrix will be at %p\n", &(mem_space[TILE_DIM*TILE_DIM*2]));
    //printf("building check matrix at %p\n", &(mem_space[TILE_DIM*TILE_DIM*3]));
    // 4th matrix = 1st matrix * 2nd matrix
    /*
    for (int i=0; i<TILE_DIM; i++) {
      for (int j=0; j<TILE_DIM; j++) {
        for (int k=0; k<TILE_DIM; k++) {
          mem_space[TILE_DIM*TILE_DIM*3+i*TILE_DIM+j] += mem_space[i*TILE_DIM+k] * mem_space[TILE_DIM*TILE_DIM + k*TILE_DIM+j];
        }
      }
    }*/
    //printf("data space initialized from %p to %p\n", mem_space, &(mem_space[TILE_DIM*TILE_DIM-1]));
    //fflush(NULL);
}

void scm_init1280() {
    printf("initializing data space starting at %p\n", SCM_MEMORY_BASE_PTR);
    unsigned char * mem_space = (unsigned char *) SCM_MEMORY_BASE_PTR;
    initMatrix((double *)&(mem_space[0]), NumElements_AB, 1);                     // initialize input 1 at root, size of 10 tiles
    initMatrix((double *)&(mem_space[B_offset]), NumElements_AB, 1);  // initialize input 2 after first, size of 10 tiles
    initMatrix((double *)&(mem_space[C_offset]), NumElements_C, 0);   // set output space to zeros after first two, size of 1 tile,
    initMatrix((double *)&(mem_space[Test_offset]), NumElements_C, 0);   // set check space to zeros after first two, size of 1 tile,
    // m, n, p : MDIM*TILE_DIM, NDIM*TILE_DIM, KDIM*TILE_DIM
    double * a = (double *)&(mem_space[0]);
    double * b = (double *)&(mem_space[B_offset]);
    double * c = (double *)&(mem_space[Test_offset]);
    for (int i=0; i<TILE_DIM; i=i+1){
      for (int j=0; j<TILE_DIM; j=j+1){
         for (int k=0; k<TILE_DIM*TILES; k=k+1){
            c[i*TILE_DIM + j]=(c[i*TILE_DIM + j])+((a[i*TILE_DIM*TILES + k])*(b[k*TILE_DIM + j]));
         }
      }
    }
}

void loadSqTile(void * dest, void * src1, void * src2) {
  double * destReg = (double *) dest;
  uint64_t address_reg = *((uint64_t *)src1);
  //uint64_t ldistance = *((uint64_t *) src2);
  uint64_t ldistance = (uint64_t) src2; //change for immediate value

  ldistance *= sizeof(double);
  for (uint64_t i=0; i<TILE_DIM; i++) {
    double *addressStart = (double *) (SCM_MEMORY_BASE_PTR + address_reg + ldistance * i); // Address L2 memory to a pointer of the runtime
    //printf("loading tile from %p to %p\n", addressStart, destReg+TILE_DIM*i);
    memcpy(destReg+TILE_DIM*i, addressStart, TILE_DIM * sizeof(double));
  }
  printf("loadSqTile ending\n");
  fflush(NULL);
}

void matMult(void * dest, void * src1, void * src2) {
  double * C = (double *) dest;
  double * A = (double *) src1;
  double * B = (double *) src2;

  for (int i=0; i<TILE_DIM; i=i+1){
    for (int j=0; j<TILE_DIM; j=j+1){
      for (int k=0; k<TILE_DIM; k=k+1) {
        C[i*TILE_DIM + j]+=((A[i*TILE_DIM + k])*(B[k*TILE_DIM + j]));
      }
      //printf("%lf ", C[i*TILE_DIM + j]);
      //fflush(NULL);
    }
    //printf(" \n");
  }
  printf("matMult ending\n");
  fflush(NULL);
}

void storeSqTile(void * dest, void * src1, void * src2) {
  // Obtaining the parameters
  uint64_t address_reg = *((uint64_t *)src1);
  //uint64_t ldistance = *((uint64_t *) src2);
  uint64_t ldistance = (uint64_t) src2;
  double *sourceReg = (double *) dest;
  ldistance *= sizeof(double);
  for (uint64_t i = 0; i < TILE_DIM; i++) {
    double *addressStart = (double *) (SCM_MEMORY_BASE_PTR + address_reg + ldistance * i); // Address L2 memory to a pointer of the runtime
    //printf("copying back to %p\n", addressStart);
    memcpy(addressStart, sourceReg+TILE_DIM*i, TILE_DIM * sizeof(double));
  }
  // after copying, do error checking in the memory space
  // note that if we change the algorithm, this will always fail because of float inaccuraccies
  /*
  double * result_mat = (double *) (SCM_MEMORY_BASE_PTR + address_reg); //index SCM mem space to where result is store
  double * check_mat = (double *) (SCM_MEMORY_BASE_PTR + address_reg + TILE_DIM*TILE_DIM*8); //index one matrix size more for the matrix made in init
   */
  bool correct = true;
  //double * result_mat = (double *) (SCM_MEMORY_BASE_PTR + address_reg);
  //double * check_mat = (double *)  (SCM_MEMORY_BASE_PTR + Test_offset);
  double * result_mat = (double *) SCM_MEMORY(address_reg);
  double * check_mat = (double *)  SCM_MEMORY(Test_offset);
  printf("reading result_mat from %p and check mat from %p\n", result_mat, check_mat);
  for (int i=0; i<TILE_DIM*TILE_DIM; i++) {
    if (result_mat[i] != check_mat[i]) {
      printf("error: element %d does not match: %lf vs %lf\n", i, result_mat[i], check_mat[i]);
      fflush(NULL);
      correct = false;
    }
  }
  if (correct) {
    printf("Matrix multiplication completed! No errors detected\n");
  }
  printf("storeSqTile ending\n");
  fflush(NULL);
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../codelet.hh"
#include "impl.hh"

#define CODELET_NUM 5

// Here the user statically allocates the space they need to initialize data
// This pointer is then passes to the SCM machine where it can manipulated based
// on offsets from the base of the space
static char SCM_MEMORY[SCM_MEM_SIZE];

// this is a statically allocated codelet graph that should be loaded into SU
// dependencies are based on the scm program and managed by the SU
// the last codelet in the list is a dummy codelet with no i/o that 
// holds in its "fire" field the base pointer of SCM memory
user_codelet_t codelet_graph[CODELET_NUM] __attribute__ ((section(".codelet_program"))) = {{OP1_WR | OP2_RD | OP3_RD, "LoadSqTile_2048L", loadSqTile},
                                                                                           {OP1_WR | OP2_RD | OP3_RD, "MatMult_2048L", matMult},
                                                                                           {OP1_RD | OP2_RD | OP3_RD, "StoreSqTile_2048L", storeSqTile},
                                                                                           {OP1_RD, "InitCod_64B", (fire_t)scm_init},
                                                                                           {0, "ScmMemBasePtr", (fire_t)SCM_MEMORY} // dummy codelet for scm memory base ptr
                                                                                           };

void scm_init() {
    // this is used to offer the users a way to do setup for a benchmark and initialize
    // the memory space that memory codelets will load from
    printf("initializing data space starting at %p\n", SCM_MEMORY);
    double * mem_space = (double *) SCM_MEMORY;
    for (int i=0; i<TILE_DIM*TILE_DIM; i++) {
      mem_space[i] = (double) i;
    }
}

void loadSqTile(void * dest, void * src1, void * src2) {
  double * destReg = (double *) dest;
  uint8_t * address_reg = (uint8_t *) src1;
  //uint64_t ldistance = *((uint64_t *) src2);
  uint64_t ldistance = (uint64_t) src2; //change for immediate value

  ldistance *= sizeof(double);
  /*
  uint64_t address = address_reg[0];
  for (int i = 1; i < 8; i++) {
    address <<= 8;
    address += address_reg[i];
  }
  */
  for (uint64_t i=0; i<TILE_DIM; i++) {
    double *addressStart = (double *) (SCM_MEMORY + ldistance * i); // Address L2 memory to a pointer of the runtime
    memcpy(destReg+TILE_DIM*i, addressStart, TILE_DIM * sizeof(double));
  }
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
    printf("col %d done\n", j);
    }
    printf("row %d done\n", i);
  }
}

void storeSqTile(void * dest, void * src1, void * src2) {
  // Obtaining the parameters
  uint8_t* address_reg = (uint8_t *) src1;
  //uint64_t ldistance = *((uint64_t *) src2);
  uint64_t ldistance = (uint64_t) src2;
  double *sourceReg = (double *) dest;

  ldistance *= sizeof(double);
  for (uint64_t i = 0; i < TILE_DIM; i++) {
    double *addressStart = (double *) (SCM_MEMORY + ldistance * i); // Address L2 memory to a pointer of the runtime
    memcpy(addressStart, sourceReg+TILE_DIM*i, TILE_DIM * sizeof(double));
  }
}
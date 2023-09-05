#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../codelet.hh"
#include "impl.hh"
#include <sys/mman.h>

#define CODELET_NUM 5

// this is a statically allocated codelet graph that should be loaded into SU
// dependencies are based on the scm program and managed by the SU
// the last codelet in the list is a dummy codelet with no i/o that 
// holds in its "fire" field the base pointer of SCM memory
user_codelet_t codelet_graph[CODELET_NUM] __attribute__ ((section(".codelet_program"))) = {{OP1_WR | OP2_RD | OP3_RD, "LoadSqTile_2048L", loadSqTile},
                                                                                           {OP1_WR | OP2_RD | OP3_RD, "MatMult_2048L", matMult},
                                                                                           {OP1_RD | OP2_RD | OP3_RD, "StoreSqTile_2048L", storeSqTile},
                                                                                           {OP1_RD, "InitCod_64B", (fire_t)scm_init},
                                                                                           {0, "ScmMemBasePtr", (fire_t)SCM_MEMORY_BASE_PTR} // dummy codelet for scm memory base ptr
                                                                                           };

void scm_init() {
    // this is used to offer the users a way to do setup for a benchmark and initialize
    // the memory space that memory codelets will load from
    printf("initializing data space starting at %p\n", SCM_MEMORY_BASE_PTR);
    //long unsigned mmap_ret = (long unsigned) mmap((void*)SCM_MEMORY, SCM_MEM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
    // removed MAP_FIXED_NOREPLACE because this stack variable is probably not on a page boundary
    //long unsigned mmap_ret = (long unsigned) mmap((void*)SCM_MEMORY_BASE_PTR, SCM_MEM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    //printf("SCM init has mmap return value %lx\n", mmap_ret);
    double * mem_space = (double *) SCM_MEMORY_BASE_PTR;
    for (int i=0; i<TILE_DIM*TILE_DIM*2; i++) {
      mem_space[i] = (double) i;
      printf("%lf ", mem_space[i]);
      fflush(NULL);
    }
    printf("data space initialized from %p to %p\n", mem_space, &(mem_space[TILE_DIM*TILE_DIM-1]));
    fflush(NULL);
}

void loadSqTile(void * dest, void * src1, void * src2) {
  double * destReg = (double *) dest;
  uint64_t address_reg = *((uint64_t *)src1);
  //uint64_t ldistance = *((uint64_t *) src2);
  uint64_t ldistance = (uint64_t) src2; //change for immediate value

  ldistance *= sizeof(double);
  printf("address reg: 0x%lx; ldistance: 0x%lx; scm memory: %p; first element: %lf\n", address_reg, ldistance, SCM_MEMORY_BASE_PTR, ((double *)SCM_MEMORY_BASE_PTR)[2]);
  fflush(NULL);
  for (uint64_t i=0; i<TILE_DIM; i++) {
    double *addressStart = (double *) (SCM_MEMORY_BASE_PTR + address_reg + ldistance * i); // Address L2 memory to a pointer of the runtime
    printf("loading tile from %p to %p\n", addressStart, destReg+TILE_DIM*i);
    memcpy(destReg+TILE_DIM*i, addressStart, TILE_DIM * sizeof(double));
  }
  for (int i=0; i<TILE_DIM; i=i+1){
    for (int j=0; j<TILE_DIM; j=j+1){
      printf("%lf ", destReg[i*TILE_DIM+j]);
      fflush(NULL);
    }
    printf("\n");
    fflush(NULL);
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
      printf("%lf ", C[i*TILE_DIM + j]);
      fflush(NULL);
    //printf("col %d done\n", j);
    }
    //printf("row %d done\n", i);
  }
}

void storeSqTile(void * dest, void * src1, void * src2) {
  // Obtaining the parameters
  //uint8_t* address_reg = (uint8_t *) src1;
  uint64_t address_reg = *((uint64_t *)src1);
  //uint64_t ldistance = *((uint64_t *) src2);
  uint64_t ldistance = (uint64_t) src2;
  double *sourceReg = (double *) dest;
  for (int i=0; i<TILE_DIM; i=i+1){
    for (int j=0; j<TILE_DIM; j=j+1){
      printf("%lf ", sourceReg[i*TILE_DIM+j]);
      fflush(NULL);
    }
    printf("\n");
    fflush(NULL);
  }
  ldistance *= sizeof(double);
  for (uint64_t i = 0; i < TILE_DIM; i++) {
    double *addressStart = (double *) (SCM_MEMORY_BASE_PTR + address_reg + ldistance * i); // Address L2 memory to a pointer of the runtime
    memcpy(addressStart, sourceReg+TILE_DIM*i, TILE_DIM * sizeof(double));
  }
}
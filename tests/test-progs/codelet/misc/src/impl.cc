#include <stdio.h>
#include <stdlib.h>
#include "../../codelet.hh"
#include "impl.hh"

#define CODELET_NUM 8

// Here the user statically allocates the space they need to initialize data
// This pointer is then passes to the SCM machine where it can manipulated based
// on offsets from the base of the space
static char SCM_MEMORY[1024];

// this is a statically allocated codelet graph that should be loaded into SU
// dependencies are based on the scm program and managed by the SU
// the last codelet in the list is a dummy codelet with no i/o that 
// holds in its "fire" field the base pointer of SCM memory
user_codelet_t codelet_graph[CODELET_NUM] __attribute__ ((section(".codelet_program"))) = {{OP1_RD, "HelloCod_2048L", helloCodFire},
                                                                                           {OP1_RD, "HelloCodTwo_2048L", helloCodFireTwo},
                                                                                           // below has RD/WR to test register copying
                                                                                           {OP1_RD | OP1_WR, "HelloCodThree_2048L", helloCodFireThree},
                                                                                           {OP1_WR, "VecInitOne_2048L", vecInitOne},
                                                                                           {OP1_WR, "VecInitTwo_2048L", vecInitTwo},
                                                                                           {OP1_WR | OP2_RD | OP3_RD, "VecAdd_2048L", vecAdd},
                                                                                           {OP1_RD, "InitCod_64B", (fire_t)scm_init},
                                                                                           {0, "ScmMemBasePtr", (fire_t)SCM_MEMORY} // dummy codelet for scm memory base ptr
                                                                                           };


void scm_init() {
    // this is used to offer the users a way to do setup for a benchmark and initialize
    // the memory space that memory codelets will load from
    printf("initializing data space starting at %p\n", SCM_MEMORY);
}

void helloCodFire(void * dest, void * src1, void * src2) {
    printf("hi from inside codelet fire function\n");
    printf("helloCod has R1 value: 0x%lx\n", *((unsigned long *)dest));
}

void helloCodFireTwo(void * dest, void * src1, void * src2) {
    printf("hi v2\n");
    printf("helloCod2 has R1 value: 0x%lx\n", *((unsigned long *)dest));
}

void helloCodFireThree(void * dest, void * src1, void * src2) {
    printf("it's hi v3 here\n");
    printf("helloCod3 has R1 %p with value 0x%lx\n", dest, *((unsigned long *)dest));
}

void vecInitOne(void * dest, void * src1, void * src2) {
    // 0x71c140 0x6dc140 0x6fc140
    // 0x71c140 -> 0x130a140.
    // la traduccion en la otra cu es incorrecta
    // 0x71c140 -> 0x71c140
    printf("running vecInitOne\n");
    printf("vecInitOne has dest: %p, src1: %p, src2: %p\n", dest, src1, src2);
    int * vecDest = (int *) dest;
    for (int i=0; i<2048; i++) {
        vecDest[i] = i;
    }
}

void vecInitTwo(void * dest, void * src1, void * src2) {
    printf("running vecInitTwo\n");
    printf("vecInitTwo has dest: %p, src1: %p, src2: %p\n", dest, src1, src2);
    int * vecDest = (int *) dest;
    for (int i=0; i<2048; i++) {
        vecDest[i] = i + i;
    }
}

void vecAdd(void * dest, void * src1, void * src2) {
    //printf("running vecAdd with src1: %p and src2: %p\n", src1, src2);
    // 0x73c140 0x71c140 0x6fc140
    // 0x7ffff7fb2730 -> 0x17f7730
    int * vecDest = (int *) dest;
    int * vecSrc1 = (int *) src1;
    int * vecSrc2 = (int *) src2;
    for (int i=0; i<2048; i++) {
        vecDest[i] = vecSrc1[i] + vecSrc2[i];
        if (i % 128 == 0) {
            printf("it %d: %d + %d = %d\n", i, vecSrc1[i], vecSrc2[i], vecDest[i]);
        }
    }
    printf("vecAdd done\n");
}

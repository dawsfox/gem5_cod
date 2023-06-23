#include <stdio.h>
#include <stdlib.h>
#include "codelet.hh"
#include "impl.hh"

#define CODELET_NUM 6
// #define REG_FILE_SIZE_KB 12288l

// this is a statically allocated codelet graph that should be loaded into SU
// dependencies are based on the scm program and managed by the SU
user_codelet_t codelet_graph[CODELET_NUM] __attribute__ ((section(".codelet_program"))) = {{"HelloCod_2048L", helloCodFire},
                                                                                           {"HelloCodTwo_2048L", helloCodFireTwo},
                                                                                           {"HelloCodThree_2048L", helloCodFireThree},
                                                                                           {"VecInitOne_2048L", vecInitOne},
                                                                                           {"VecInitTwo_2048L", vecInitTwo},
                                                                                           {"VecAdd_2048L", vecAdd}};


void helloCodFire(void * dest, void * src1, void * src2) {
    printf("hi from inside codelet fire function\n");
}

void helloCodFireTwo(void * dest, void * src1, void * src2) {
    printf("hi v2\n");
}

void helloCodFireThree(void * dest, void * src1, void * src2) {
    printf("it's hi v3 here\n");
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

#include "codelet.hh"
#include <stdio.h>
#include <stdlib.h>
//#include <type_traits>

#define INTERFACE_ACTIVE_COD_PTR 0x90000000
#define INTERFACE_COD_AVAIL_PTR INTERFACE_ACTIVE_COD_PTR + sizeof(runt_codelet_t)

// this is a statically allocated codelet graph that should be loaded into SU
// later, this will be syncSlots instead of just codelets
#define CODELET_NUM 6
#define REG_FILE_SIZE_KB 12288l

/*
codelet_t codelet_graph[CODELET_NUM] __attribute__ ((section(".codelet_program"))) = {{helloCodFire, 0, 0, 0, 0},
                                                                            {helloCodFireTwo, 7, 0, 0, 1}, 
                                                                            {helloCodFire, 3, 2, 1, 2}, 
                                                                            {helloCodFire, 4, 3, 2, 3}, 
                                                                            {helloCodFireTwo, 2, 4, 2, 4},
                                                                            {helloCodFireThree, 5, 4, 2, 3}
                                                                            };
*/

user_codelet_t codelet_graph[CODELET_NUM] __attribute__ ((section(".codelet_program"))) = {{"HelloCod_2048L", helloCodFire},
                                                                                           {"HelloCodTwo_2048L", helloCodFireTwo},
                                                                                           {"HelloCodThree_2048L", helloCodFireThree},
                                                                                           {"VecInitOne_2048L", vecInitOne},
                                                                                           {"VecInitTwo_2048L", vecInitTwo},
                                                                                           {"VecAdd_2048L", vecAdd}};

unsigned char register_space[REG_FILE_SIZE_KB * 1000];

unsigned char * register_space_ptr __attribute__ ((section(".register_space_ptr"))) = register_space;

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
    for (int i=0; i<8; i++) {
        vecDest[i] = i;
    }
}

void vecInitTwo(void * dest, void * src1, void * src2) {
    printf("running vecInitTwo\n");
    printf("vecInitTwo has dest: %p, src1: %p, src2: %p\n", dest, src1, src2);
    int * vecDest = (int *) dest;
    for (int i=0; i<8; i++) {
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
    for (int i=0; i<8; i++) {
        vecDest[i] = vecSrc1[i] + vecSrc2[i];
        //printf("it %d: %d + %d = %d\n", i, vecSrc1[i], vecSrc2[i], vecDest[i]);
    }
    printf("vecAdd done\n");
}

int main(int argc, char* argv[])
{
    printf("main -- begin\n");
    unsigned cu_id = 0;
    if(argc > 1) {
        char * cu_id_str = (char *)argv[1];
        cu_id = atoi(cu_id_str);
    }
    printf("main -- found CU id %d\n", cu_id);
    bool alive_sig = true;
    printf("register space has address %p\n", register_space);
    volatile unsigned * codeletAvailable;
    volatile runt_codelet_t * toFire;
    while(alive_sig) {
        // increment the CodeletInterface-based addresses by 0x40 based on CU id
        codeletAvailable = (volatile unsigned *) (((char *)INTERFACE_COD_AVAIL_PTR) + cu_id * 0x44U);
        toFire = (volatile runt_codelet_t *) (((char *)INTERFACE_ACTIVE_COD_PTR) + cu_id * 0x44U);
        if (*codeletAvailable) {
            printf("CU %d: codelet available = %x\n", cu_id, *codeletAvailable);
            printf("CU %d: fire = %p\n", cu_id, (void *)toFire->fire);
            printf("CU %d: codelet name: %s\n", cu_id, toFire->name);
            if (toFire->fire != nullptr && toFire->fire != (fire_t)0xffffffffffffffff) {
                toFire->fire(toFire->dest, toFire->src1, toFire->src2);
                // this should perform a write operation to the activeCodelet in the CodeletInterface
                // which should be accepted as CodeletRetirement
                toFire->fire = nullptr;
            } else if (toFire->fire == (fire_t)0xffffffffffffffff) {
                alive_sig = false;
                printf("CU %d: final codelet received\n", cu_id);
            }
        }
    }
    printf("CU %d: program returning\n", cu_id);
    return(0);
}
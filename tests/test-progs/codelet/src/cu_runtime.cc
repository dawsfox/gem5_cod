#include "codelet.hh"
#include "stdio.h"
//#include <type_traits>

#define interface_queue_addr 0x90000002

// this is a statically allocated codelet graph that should be loaded into SU
// later, this will be syncSlots instead of just codelets
#define CODELET_NUM 3
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
                                                                                           {"HelloCodThree_2048L", helloCodFireThree}};

unsigned char register_space[REG_FILE_SIZE_KB * 1000];

unsigned char * register_space_ptr __attribute__ ((section(".register_space_ptr"))) = register_space;

void helloCodFire(void * dest, void * src1, void * src2) {
    printf("hi from inside codelet fire function\n");
}

void helloCodFireTwo(void * dest, void * src1, void * src2) {
    printf("hi v2\n");
}

void helloCodFireThree(void * dest, void * src1, void * src2) {
    printf("it's v3 here\n");
}

int main(int argc, char* argv[])
{
    bool alive_sig = true;
    printf("register space has address %p\n", register_space);
    while(alive_sig) {
        //volatile fire_t fire;
        //fire = *(fire_t *)interface_queue_addr;
        //asm volatile("mov %1, (%0)" : "=r" (tmp_cod.fire) : "r" (interface_queue_addr));
        runt_codelet_t toFire;
        toFire = *(runt_codelet_t *)interface_queue_addr;
        printf("fire = %p\n", (void *)toFire.fire);
        printf("codelet name: %s\n", toFire.name);
        if (toFire.fire != nullptr && toFire.fire != (fire_t)0xffffffffffffffff) {
            toFire.fire(toFire.dest, toFire.src1, toFire.src2);
        } else if (toFire.fire == (fire_t)0xffffffffffffffff) {
            alive_sig = false;
            printf("program ending\n");
        }
    }
    printf("program returning\n");
    return(0);
}
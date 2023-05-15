#include "codelet.hh"
#include "stdio.h"
//#include <type_traits>

#define interface_queue_addr 0x90000002

// this is a statically allocated codelet graph that should be loaded into SU
// later, this will be syncSlots instead of just codelets
#define CODELET_NUM 6

/*
codelet_t codelet_graph[CODELET_NUM] __attribute__ ((section(".codelet_program"))) = {{helloCodFire, 0, 0, 0, 0},
                                                                            {helloCodFireTwo, 7, 0, 0, 1}, 
                                                                            {helloCodFire, 3, 2, 1, 2}, 
                                                                            {helloCodFire, 4, 3, 2, 3}, 
                                                                            {helloCodFireTwo, 2, 4, 2, 4},
                                                                            {helloCodFireThree, 5, 4, 2, 3}
                                                                            };
*/

user_codelet_t codelet_graph[CODELET_NUM] __attribute__ ((section(".codelet_program"))) = {{"HelloCodelet1.1", helloCodFire},
                                                                                           {"HelloCodelet2.1", helloCodFireTwo},
                                                                                           {"HelloCodelet1.2", helloCodFire},
                                                                                           {"HelloCodelet1.3", helloCodFire},
                                                                                           {"HelloCodelet2.2", helloCodFireTwo},
                                                                                           {"HelloCodelet3.1", helloCodFireThree}};

void helloCodFire() {
    printf("hi from inside codelet fire function\n");
}

void helloCodFireTwo() {
    printf("hi v2\n");
}

void helloCodFireThree() {
    printf("it's v3 here\n");
}

int main(int argc, char* argv[])
{
    bool alive_sig = true;
    printf("helloCodFire is located at %p\n", (void *)helloCodFire);
    while(alive_sig) {
    //for (int i=0; i<10; i++) {
        volatile fire_t fire;
        fire = *(fire_t *)interface_queue_addr;
        //asm volatile("mov %1, (%0)" : "=r" (tmp_cod.fire) : "r" (interface_queue_addr));
        printf("fire = %p\n", (void *)fire);
        if (fire != nullptr && fire != (fire_t)0xffffffffffffffff) {
            fire();
        } else if (fire == (fire_t)0xffffffffffffffff) {
            alive_sig = false;
            printf("program ending\n");
        }
    }
    printf("program returning\n");
    return(0);
}
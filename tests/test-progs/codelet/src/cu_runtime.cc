#include "codelet.hh"
#include "stdio.h"

#define interface_queue_addr 0x90000002

// this is a statically allocated codelet graph that should be loaded into SU
// later, this will be syncSlots instead of just codelets
codelet_t codelet_graph[5] __attribute__ ((section(".codelet_program"))) = {{helloCodFire, 0},
                                                                            {helloCodFire, 1}, 
                                                                            {helloCodFire, 2}, 
                                                                            {helloCodFire, 3}, 
                                                                            {helloCodFire, 4} 
                                                                            };

void helloCodFire() {
    printf("hi from inside codelet fire function\n");
}

int main(int argc, char* argv[])
{
    for (int i=0; i<35; i++) {
        codelet_t tmp_cod;
        tmp_cod.fire = *(fire_t *)interface_queue_addr;
        //asm volatile("mov %1, (%0)" : "=r" (tmp_cod.fire) : "r" (interface_queue_addr));
        if (tmp_cod.fire != nullptr) {
            tmp_cod.fire();
        } else {
            printf("received null codelet\n");
        }

        printf("done popping\n");
    }
    return(0);
}
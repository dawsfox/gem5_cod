#include "codelet.hh"
#include "stdio.h"

#define interface_queue_addr 0x900000002

void helloCodFire() {
    printf("hi from inside codelet fire function\n");
}

int main(int argc, char* argv[])
{
    codelet_t tmp_cod;
    tmp_cod = *(codelet_t *)interface_queue_addr;
    //popped.fire();
    asm volatile("mov %1, (%0)" : "=r" (tmp_cod.fire) : "r" (interface_queue_addr));

    printf("done popping\n");
    return(0);
}
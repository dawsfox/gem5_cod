#include "codelet.hh"
#include <stdio.h>
#include <stdlib.h>
#include "impl.hh"

//unsigned char register_space[REG_FILE_SIZE_KB * 1000];
//unsigned char * register_space_ptr __attribute__ ((section(".register_space_ptr"))) = register_space;

int main(int argc, char* argv[])
{
    unsigned cu_id = 0;
    if(argc > 1) {
        char * cu_id_str = (char *)argv[1];
        cu_id = atoi(cu_id_str);
    }
    printf("main -- found CU id %d\n", cu_id);
    bool alive_sig = true;
    //printf("register space has address %p\n", register_space);
    volatile unsigned * codeletAvailable;
    volatile runt_codelet_t * toFire;
    printf("CU %d: toFire @ %p; codeletAvailable @ %p\n", cu_id, ((char *)INTERFACE_ACTIVE_COD_PTR) + cu_id * 0x44U, ((char *)INTERFACE_COD_AVAIL_PTR) + cu_id * 0x44U);
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
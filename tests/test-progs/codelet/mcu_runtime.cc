#include <stdio.h>
#include <stdlib.h>
#include "codelet.hh"

int main(int argc, char* argv[])
{
    unsigned cu_id = 0;
    if(argc > 1) {
        char * cu_id_str = (char *)argv[1];
        cu_id = atoi(cu_id_str);
    }
    bool alive_sig = true;
    volatile unsigned * codeletAvailable;
    volatile runt_memcod_t * toFire;
    codeletAvailable = MEMCOD_AVAILABLE(cu_id);
    toFire = MEMCOD_BASE(cu_id);
    while(alive_sig) {
        if (*codeletAvailable) {
            //printf("CU %d: codelet available = %x\n", cu_id, *codeletAvailable);
            printf("MCU with id %d: rng_res = %p\n", cu_id, (void *)toFire->rng_res);
            fflush(NULL);
            //printf("CU %d: codelet name: %s\n", cu_id, toFire->name);
            if (toFire->rng_res != nullptr && toFire->fire != (fire_t)0xffffffffffffffff) {
                // the rng_res function computes the memory ranges that will be touched by this
                // memory codelet when it is firing. It is expected to repeatedly write the
                // MEMRANGE_BASE(cu_id) with the base of a range, the MEMRANGE_SIZE(cu_id) with
                // the size of a range, and the MEMRANGE_SUBMIT(cu_id) to load it to the memcod
                // interface until all the ranges are resolved. Write a 1U to it if the range
                // submitted is a write range, 0U if it is a read range.
                toFire->rng_res(toFire->dest, toFire->src1, toFire->src2);
                // this should perform a write operation to the activeCodelet in the CodeletInterface
                // which should be accepted as CodeletRetirement
                *((volatile unsigned *)&(toFire->fire)) = 0;
            } else if (toFire->fire == (fire_t)0xffffffffffffffff) {
                alive_sig = false;
                //printf("CU %d: final codelet received\n", cu_id);
            }
        }
    }
    //printf("CU %d: program returning\n", cu_id);
    return(0);
}
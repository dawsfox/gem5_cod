#include <stdio.h>
#include <stdlib.h>
#include "../../codelet.hh"
#include "impl.hh"

#define CODELET_NUM 1

// this is a statically allocated codelet graph that should be loaded into SU
// dependencies are based on the scm program and managed by the SU
user_codelet_t codelet_graph[CODELET_NUM] __attribute__ ((section(".codelet_program"))) = {{"EmptyCod_R64B", emptyCodFire}
                                                                                           };


void emptyCodFire(void * dest, void * src1, void * src2) {
    //printf("codelet firing\n");
    return;
}

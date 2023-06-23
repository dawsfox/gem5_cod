#ifndef __CODELET_CODELET_HH__
#define __CODELET_CODELET_HH__

#define INTERFACE_ACTIVE_COD_PTR 0x90000000
#define INTERFACE_COD_AVAIL_PTR INTERFACE_ACTIVE_COD_PTR + sizeof(runt_codelet_t)

// typedef for fire function (function pointer)
typedef void (*fire_t)(void * dest, void * src1, void * src2);

/* This structure is used when the user is creating Codelets that will be referenced in the Codelet program
   This structure will be loaded from the elf file into the SU 
   The SU will then reference this to get the fire functions of each codelet, build the runt(ime) codelet
   with valid params, and send it to CUs for execution */
typedef struct user_codelet_s {
    char name[32];
    fire_t fire;
} user_codelet_t;

/* This structure is needed so that:
   - The user can create Codelets connected to a certain function
   - The SU can connect the fire function to the Codelet in the SCM program by name
   - The SU can provide memory locations for input and output
*/
typedef struct runt_codelet_s {
    fire_t fire = nullptr;
    void * src1 = nullptr;
    void * src2 = nullptr;
    void * dest = nullptr;
    char name[32];
} runt_codelet_t;
// the above struct should replace codelet_t in the CU runtime
// also, this WHOLE codelet will have to be copied (the current gem5 side only sends a fire_t back)
// otherwise, the params will not be set correctly

// note, the codelet structure needs a name to connect back
// to the Codelet Model program that will be read into the SU
typedef struct codelet_s {
    fire_t fire = nullptr;
    unsigned int dest;
    unsigned int src1;
    unsigned int src2;
    unsigned id;
    // 64 is a good number because it won't cause padding
    char name[32];
} codelet_t;

#endif
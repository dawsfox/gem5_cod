#ifndef __CODELET_CODELET_HH__
#define __CODELET_CODELET_HH__

#include <stdint.h>

#define INTERFACE_ACTIVE_COD_PTR 0x90000000
#define INTERFACE_COD_AVAIL_PTR INTERFACE_ACTIVE_COD_PTR + sizeof(runt_codelet_t)

#define SCM_MEMORY_BASE_PTR ((unsigned char *) 0x91771000)
#define SCM_MEMORY(offset) ((SCM_MEMORY_BASE_PTR) + (offset))

#define CODELET_AVAILABLE(cu_id)  ((volatile unsigned *) (((char *)INTERFACE_COD_AVAIL_PTR) + (cu_id) * (sizeof(runt_codelet_t) + sizeof(unsigned))))
#define TO_FIRE(cu_id) ((volatile runt_codelet_t *) (((char *)INTERFACE_ACTIVE_COD_PTR) + (cu_id) * (sizeof(runt_codelet_t) + sizeof(unsigned))))

//below define statements are for the mcu runtime where cu_id will be the highest of all cores
#define MEMCOD_BASE(cu_id) ((volatile runt_memcod_t *) (((char *)INTERFACE_ACTIVE_COD_PTR) + (cu_id) * (sizeof(runt_codelet_t) + sizeof(unsigned))))
// used for checking if there is a memcod waiting in queue
#define MEMCOD_AVAILABLE(cu_id) ((volatile unsigned *) (((char *)MEMCOD_BASE(cu_id)) + sizeof(runt_memcod_t)))
// used for writing the start value of a memrange
#define MEMRANGE_BASE(cu_id) (volatile long unsigned *) (((char *)MEMCOD_BASE(cu_id))+(sizeof(runt_memcod_t) + sizeof(unsigned)))
// used for writing the size of the memrange
#define MEMRANGE_SIZE(cu_id) (volatile unsigned *) (((char *)MEMRANGE_BASE(cu_id))+(sizeof(long unsigned)))
// used for signaling to memcod interface that the range is complete and should be added to active memranges
#define MEMRANGE_SUBMIT(cu_id) (volatile unsigned *) (((char *)MEMRANGE_SIZE(cu_id))+(sizeof(unsigned)))

// typedef for fire function (function pointer)
typedef void (*fire_t)(void * dest, void * src1, void * src2);

static constexpr uint16_t NO_RD_WR  { 0b0000'0000'0000'0000 }; // represents bit 0
static constexpr uint16_t OP1_RD    { 0b0000'0000'0000'0001 }; // represents bit 1
static constexpr uint16_t OP1_WR    { 0b0000'0000'0000'0010 }; // represents bit 2
static constexpr uint16_t OP2_RD    { 0b0000'0000'0000'0100 }; // represents bit 3
static constexpr uint16_t OP2_WR    { 0b0000'0000'0000'1000 }; // represents bit 4
static constexpr uint16_t OP3_RD    { 0b0000'0000'0001'0000 }; // represents bit 5
static constexpr uint16_t OP3_WR    { 0b0000'0000'0010'0000 }; // represents bit 6
static constexpr uint16_t OP4_RD    { 0b0000'0000'0100'0000 }; // represents bit 7
static constexpr uint16_t OP4_WR    { 0b0000'0000'1000'0000 }; // represents bit 8
static constexpr uint16_t OP5_RD    { 0b0000'0001'0000'0000 }; // represents bit 9
static constexpr uint16_t OP5_WR    { 0b0000'0010'0000'0000 }; // represents bit 10
static constexpr uint16_t OP6_RD    { 0b0000'0100'0000'0000 }; // represents bit 11
static constexpr uint16_t OP6_WR    { 0b0000'1000'0000'0000 }; // represents bit 12
static constexpr uint16_t OP7_RD    { 0b0001'0000'0000'0000 }; // represents bit 13
static constexpr uint16_t OP7_WR    { 0b0010'0000'0000'0000 }; // represents bit 14
static constexpr uint16_t OP8_RD    { 0b0100'0000'0000'0000 }; // represents bit 15
static constexpr uint16_t OP8_WR    { 0b1000'0000'0000'0000 }; // represents bit 16

/* This structure is used when the user is creating Codelets that will be referenced in the Codelet program
   This structure will be loaded from the elf file into the SU 
   The SU will then reference this to get the fire functions of each codelet, build the runt(ime) codelet
   with valid params, and send it to CUs for execution */
typedef struct user_codelet_s {
    uint16_t io;
    char name[30];
    fire_t fire;
} user_codelet_t;

/* This structure is used when the user is creating Memory Codelets that will be referenced in the Codelet program
   Practically, it is handled the same as Codelets except (1) it involves and extra fire_t field that points to a function
   used to resolve the Memory Ranges that will be touched by this Memory Codelet (calculated at schedule-time) and
   (2) Memory Codelets are only deployed to MCU threads and (3) once scheduled, they cannot be executed until it is ensured
   that there is no active Memory Range conflict */
typedef struct user_memcod_s {
    uint16_t io;
    char name[30];
    fire_t fire;
    fire_t rng_res;
} user_memcod_t;

/* This is the memory codelet version of the runtime codelet structure. It includes a rng_res function for 
   resolving at schedule-time the memory ranges that a memcod will read/write for consistency management. */
typedef struct runt_memcod_s {
    fire_t fire = nullptr;
    fire_t rng_res = nullptr;
    void * src1 = nullptr;
    void * src2 = nullptr;
    void * dest = nullptr;
    char name[32];
    uint64_t unid; //unique id
} runt_memcod_t;

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
    uint64_t unid;
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
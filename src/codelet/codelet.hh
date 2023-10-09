#ifndef __CODELET_CODELET_HH__
#define __CODELET_CODELET_HH__

namespace gem5
{

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

typedef struct user_codelet_s {
    uint16_t io;
    char name[30];
    fire_t fire;
} user_codelet_t;

typedef struct user_memcod_s {
    uint16_t io;
    char name[30];
    fire_t fire;
    fire_t rng_res; // (memory) range resolution function
} user_memcod_t;


typedef struct runt_codelet_s {
    fire_t fire = nullptr;
    void * src1 = nullptr;
    void * src2 = nullptr;
    void * dest = nullptr;
    char name[32];
    uint64_t unid; //unique id
} runt_codelet_t;

typedef struct runt_memcod_s {
    fire_t fire = nullptr;
    fire_t rng_res = nullptr;
    void * src1 = nullptr;
    void * src2 = nullptr;
    void * dest = nullptr;
    char name[32];
    uint64_t unid; //unique id
} runt_memcod_t;


typedef struct retire_data_s {
    runt_codelet_t toRet;
    unsigned cuId;
}  retire_data_t;


} // namespace gem5

#endif
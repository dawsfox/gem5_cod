#ifndef __CODELET_CODELET_HH__
#define __CODELET_CODELET_HH__

namespace gem5
{
    /*
class Codelet 
{
    private:
        unsigned id;
    public:
        virtual void fire();
        Codelet(unsigned id) :
            id(id)
            { }

}; //class Codelet
*/

typedef void (*fire_t)();

typedef struct user_codelet_s {
    char name[32];
    fire_t fire;
} user_codelet_t;

typedef struct codelet_s {
    fire_t fire = nullptr;
    unsigned int dest;
    unsigned int src1;
    unsigned int src2;
    unsigned int id;
    // 64 is a good number because it won't cause padding
    char name[32];
} codelet_t;

typedef struct runt_codelet_s {
    fire_t fire = nullptr;
    void * src1 = nullptr;
    void * src2 = nullptr;
    void * dest = nullptr;
    char name[32];
} runt_codelet_t;


} // namespace gem5

#endif
#ifndef __CODELET_CODELET_HH__
#define __CODELET_CODELET_HH__

/*
class Codelet 
{
    private:
        unsigned id;
    public:
        // replace virtual function w/ function pointer
        virtual void fire();
        Codelet(unsigned id) :
            id(id)
            { }

}; //class Codelet
*/
// typedef for fire function (function pointer)
typedef void (*fire_t)();

typedef struct codelet_s {
    //void (*fire)() = nullptr;
    fire_t fire = nullptr;
    unsigned id;
} codelet_t;

// define a tester function here
void helloCodFire();

#endif
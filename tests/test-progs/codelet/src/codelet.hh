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
// typedef for dependency list (list of ID numbers)
typedef struct dep_list_s {
    unsigned int dest;
    unsigned int src1;
    unsigned int src2;
} dep_list_t;

/*
typedef struct codelet_s {
    fire_t fire = nullptr;
    unsigned id;
} codelet_t;
*/

typedef struct codelet_s {
    fire_t fire = nullptr;
    unsigned int dest;
    unsigned int src1;
    unsigned int src2;
    unsigned id;
} codelet_t;

/*
class Codelet {
    private:
        //codelet_t cod_data;
        fire_t fire;
        unsigned id;
        unsigned dest;
        unsigned src1;
        unsigned src2;
    public:
        Codelet(fire_t fire, unsigned int id) :
            fire(fire), id(id)
            { }
        
        Codelet(fire_t fire, unsigned int dest, unsigned int id, unsigned int src1, unsigned int src2) :
            fire(fire), id(id), dest(dest), src1(src1), src2(src2)
            { }
};
 */

// define a tester function here
void helloCodFire();
void helloCodFireTwo();
void helloCodFireThree();

#endif
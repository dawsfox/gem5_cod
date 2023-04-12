#ifndef __CODELET_CODELET_HH__
#define __CODELET_CODELET_HH__

namespace gem5
{
class Codelet 
{
    private:
        void * fireFunc;
        unsigned id;
    public:
        Codelet(void * fireFunc, unsigned id) :
            fireFunc(fireFunc), id(id)
            { }

}; //class Codelet
} // namespace gem5

#endif
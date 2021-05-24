#ifndef __CPU_O3_DOM_HH__
#define __CPU_O3_DOM_HH__

#include "arch/registers.hh"
#include "base/types.hh"
#include "config/the_isa.hh"

template <class Impl>
class DOM
{
    public:
        typedef typename Impl::O3CPU O3CPU;
        typedef typename Impl::DynInstPtr DynInstPtr;

        DOM(O3CPU *_cpu, const DerivO3CPUParams &params);

        void insertLoad(const DynInstPtr &inst, ThreadID tid);
        void insertBranch(const DynInstPtr &inst, ThreadID tid);
        void safeBranch(const DynInstPtr &inst, ThreadID tid);
        void mispredictBranch(const DynInstPtr &inst, ThreadID tid);

        bool isSbFull(ThreadID tid)
        { return sbList[tid].size() == maxNumSbEntries; }

        bool isRqFull (ThreadID tid)
        { return rqList[tid].size() == maxNumRqEntries; }

        std::string name() const;


    private:

        O3CPU *cpu;

        std::list<ThreadID> *activeThreads;

        unsigned maxNumSbEntries;
        unsigned maxNumRqEntries;

        unsigned sbHead;
        unsigned sbTail;
        unsigned width;

        std::array<DynInstPtr, maxNumSbEntries> sbList[Impl::MaxThreads];
        std::queue<std::pair<int, DynInstPtr>> rqList[Impl::MaxThreads];

        bool tagBetweenHeadAndTail(int sbTag);

        void stepRq(ThreadID tid);
        void stepSb(ThreadID tid);

        void tick();


        ThreadID numThreads;

        void resetState();

};

#endif //__CPU_O3_DOM_HH__

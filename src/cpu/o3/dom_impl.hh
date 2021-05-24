#ifndef __CPU_O3_ROB_IMPL_HH
#define __CPU_O3_ROB_IMPL_HH

#include "base/types.hh"
#include "cpu/o3/dom.hh"
#include "params/DerivO3CPU.hh"

template <class Impl>
DOM<Impl>::DOM(O3CPU *_cpu, const DerivO3CPUParams &params)
    : cpu(_cpu),
    maxNumSbEntries(params.numSbEntries),
    maxNumRqEntries(params.numRqEntries),
    width(params.decodeWidth)
{

}

template <class Impl>
void
DOM<Impl>::resetState()
{
    auto numThreads = activeThreads->size();

    std::list<ThreadID>::iterator threads = activeThreads->begin();
    std::list<ThreadID>::iterator end = activeThreads->end();

    while (threads != end) {
        ThreadID tid = *threads++;


    }
}

template <class Impl>
std::string
DOM<Impl>::name() const
{
    return cpu->name()+ ".dom";
}

template <class Impl>
void
DOM<Impl>::insertBranch(const DynInstPtr &inst, ThreadID tid)
{
    sbList[tid][sbTail] = inst;
    sbTail = (sbTail + 1) % maxSbEntries
}

template <class Impl>
void
DOM<Impl>::insertLoad(const DynInstPtr &inst, ThreadID tid)
{
    if (sbHead != sbTail) {
        rqList[tid].push({sbTail, inst});
    }
}

template <class Impl>
void
DOM<Impl>::safeBranch(const DynInstPtr &inst, ThreadID tid)
{
    auto index = std::find(std::begin(sbList), std::end(sbList), inst);
    sbList[index] = 0;
}

template <class Impl>
void
DOM<Impl>::mispredictBranch(const DynInstPtr &inst, ThreadID tid)
{
    auto index = std::find(std::begin(sbList), std::end(sbList), inst);
    sbList[index] = 0;
    sbTail = index;
}

template <class Impl>
void
DOM<Impl>::stepSb(ThreadID tid)
{
    if ((sbList[sbHead] == 0) && sbHead != sbTail) {
        sbHead = (sbHead + 1) % maxSbEntries;
    }
}

template <class Impl>
bool
DOM<Impl>::tagBetweenHeadAndTail(int sbTag)
{
    if (sbHead < sbTail) {
        return sbHead <= sbTag && sbTag < sbTail;
    }
    return sbHead <= sbTag || sbTag < sbTail;
}

template <class Impl>
void
DOM<Impl>::stepRq(ThreadID tid)
{
    if ((tagBetweenHeadAndTail(rqList.front()[0]))) {
        rqList.pop();
    }
}

template <class Impl>
void
DOM<Impl>::tick()
{
    for (int i = 0; i < activeThreads->size(); i++) {
        for (int j = 0; j < width; j++) {
            stepSb(i);
        }
        for (int j = 0; j < width; j++) {
            stepRq(i);
        }
    }
}



#endif//__CPU_O3_DOM_IMPL_HH

#ifndef __CPU_O3_DOM_HH__
#define __CPU_O3_DOM_HH__

#include <tuple>

#include "arch/registers.hh"
#include "arch/types.hh"
#include "base/statistics.hh"
#include "base/stats/group.hh"
#include "base/types.hh"
#include "config/the_isa.hh"
#include "cpu/inst_seq.hh"
#include "debug/DOM.hh"
#include "debug/DebugDOM.hh"

struct DerivO3CPUParams;

template<class Impl>
class DefaultDOM
{
    private:
        typedef typename Impl::CPUPol CPUPol;
        typedef typename Impl::DynInstPtr DynInstPtr;
        typedef typename Impl::O3CPU O3CPU;

        O3CPU *cpu;

        std::list<ThreadID> *activeThreads;

        int maxNumSbEntries;
        int maxNumRqEntries;

        int sbHead[Impl::MaxThreads];
        int sbTail[Impl::MaxThreads];
        int width;

        ThreadID numThreads;

        void resetState();
    public:

        DefaultDOM(O3CPU *_cpu, const DerivO3CPUParams &params);


        void insertLoad(const DynInstPtr &inst, ThreadID tid);
        void insertBranch(const DynInstPtr &inst, ThreadID tid);
        void safeBranch(const DynInstPtr &inst, ThreadID tid);
        void mispredictBranch(const DynInstPtr &inst, ThreadID tid);

        void squashInstruction(const DynInstPtr &inst, ThreadID tid);
        void squashThread(ThreadID tid);
        void squashFromInstSeqNum(InstSeqNum seqNum, ThreadID tid);

        bool isSbFull(ThreadID tid);
        bool isRqFull (ThreadID tid);

        void setActiveThreads(std::list<ThreadID> *at_ptr);

        std::string name() const;

        void tick();
    private:
        bool tagCheck(int sbTag, ThreadID tid);
        void restoreFromIndex(ThreadID tid);

        void stepRq(ThreadID tid);
        void stepSb(ThreadID tid);

        int getBranchIndex(const DynInstPtr &inst, ThreadID tid);
        int getLoadIndex(const DynInstPtr &inst, ThreadID tid);

        void clearDeadEntries();


        std::vector<std::tuple<DynInstPtr, int, bool>>
            sbList[Impl::MaxThreads];
        std::vector<std::tuple<int, DynInstPtr>> rqList[Impl::MaxThreads];

        struct DOMStats : public Stats::Group
        {
            DOMStats(O3CPU *cpu);

            Stats::Scalar emptyCycles;
            Stats::Scalar activeCycles;
            Stats::Scalar branchesInserted;
            Stats::Scalar branchesCleared;
            Stats::Scalar timesMispredicted;
            Stats::Scalar entriesSquashed;
            Stats::Scalar loadsSquashed;
            Stats::Scalar loadsInserted;
            Stats::Scalar loadsCleared;
            Stats::Scalar abnormalBranches;
            Stats::Scalar abnormalLoads;
        } domStats;
};

#endif //__CPU_O3_DOM_HH__

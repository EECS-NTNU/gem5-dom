#ifndef __CPU_O3_TAINT_TRACKER_HH__
#define __CPU_O3_TAINT_TRACKER_HH__

#include <map>

#include "arch/registers.hh"
#include "arch/types.hh"
#include "base/statistics.hh"
#include "base/stats/group.hh"
#include "base/types.hh"
#include "config/the_isa.hh"
#include "cpu/inst_seq.hh"
#include "cpu/reg_class.hh"

struct DerivO3CPUParams;

template<class Impl>
class DefaultTaintTracker
{
    private:
        typedef typename Impl::CPUPol CPUPol;
        typedef typename Impl::DynInstPtr DynInstPtr;
        typedef typename Impl::O3CPU O3CPU;

        O3CPU *_cpu;

        std::map<PhysRegIdPtr, DynInstPtr> taints;

    public:
        void resetState();

        void insertTaint(PhysRegIdPtr &regId, DynInstPtr &load_inst);

        bool isTainted(PhysRegIdPtr regId);

        DynInstPtr getTaintInstruction(PhysRegIdPtr regId);

        void pruneTaints();

        DefaultTaintTracker(O3CPU *_cpu, const DerivO3CPUParams &params);

    private:

        struct TaintTrackerStats : public Stats::Group
        {
            TaintTrackerStats(O3CPU *cpu);

            Stats::Scalar taintsInserted;
            Stats::Scalar taintsFreed;
        } stats;

};

#endif //_CPU_O3_TAINT_TRACKER_HH__

#ifndef __CPU_O3_TAINT_TRACKER_IMPL_HH__
#define __CPU_O3_TAINT_TRACKER_IMPL_HH__

#include "cpu/o3/taint_tracker.hh"
#include "debug/TaintTrackerDebug.hh"

template<class Impl>
DefaultTaintTracker<Impl>::DefaultTaintTracker(
    O3CPU *_cpu,
    const DerivO3CPUParams &params)
    : _cpu(_cpu),
    stats(_cpu)
{

}

template<class Impl>
void
DefaultTaintTracker<Impl>::resetState()
{
    taints.clear();
}

template<class Impl>
void
DefaultTaintTracker<Impl>::insertTaint(
    PhysRegIdPtr &regId, DynInstPtr &load_inst)
{
    ++stats.taintsInserted;
    DPRINTF(TaintTrackerDebug, "Inserted new taint: "
            "regId: %d, [sn:%llu]\n",
            regId->flatIndex(),
            load_inst->seqNum);
    taints.insert(std::make_pair(regId, load_inst));
}

template<class Impl>
bool
DefaultTaintTracker<Impl>::isTainted(PhysRegIdPtr regId)
{
    auto it = taints.find(regId);
    return it != taints.end();
}


template <class Impl>
bool
DefaultTaintTracker<Impl>::hasTaintedSrc(const DynInstPtr &inst)
{
    DPRINTF(TaintTrackerDebug,
            "Checking for tainted source for [sn:%llu]\n",
            inst->seqNum);
    if (!_cpu->STT) return false;
    assert(inst->isControl() || inst->isLoad());
    for (int src_reg_idx = 0;
            src_reg_idx < inst->numSrcRegs();
            src_reg_idx++)
    {
        PhysRegIdPtr regId = inst->regs.renamedSrcIdx(src_reg_idx);
        if (isTainted(regId)) {
            return true;
        }
    }
    return false;
}

template<class Impl>
typename Impl::DynInstPtr
DefaultTaintTracker<Impl>::getTaintInstruction(PhysRegIdPtr regId)
{
    auto it = taints.find(regId);
    assert(it != taints.end());
    return it->second;
}

template<class Impl>
void
DefaultTaintTracker<Impl>::pruneTaints()
{
    if (taints.size() == 0) return;
    auto it = taints.begin();
    std::queue<PhysRegIdPtr> toDelete;
    std::list<int> orderedRegs;
    long oldest = it->second->seqNum;
    DPRINTF(TaintTrackerDebug, "Pruning taints, currTaints: %d\n",
            taints.size());
    while (it != taints.end()) {
        if ( (!it->second->underShadow()) ||
             it->second->isSquashed() ||
             it->second->isCommitted() ) {

            assert(!it->second->isCommitted());

            DPRINTF(TaintTrackerDebug, "Clearing taint "
                    "[RegId:%d]:[sn:%llu], squashed: %d\n",
                    it->first->flatIndex(),
                    it->second->seqNum,
                    it->second->isSquashed());
            toDelete.push(it->first);
        }
        if (it->second->seqNum < oldest)
            oldest = it->second->seqNum;
        orderedRegs.push_back(it->first->flatIndex());
        it++;
    }

    int clearedTaints = toDelete.size();
    while (toDelete.size() != 0) {
        PhysRegIdPtr elem = toDelete.front();
        taints.erase(elem);
        toDelete.pop();
        ++stats.taintsFreed;
    }
    DPRINTF(TaintTrackerDebug, "ClearedTaints: %d, "
            "currTaints: %d, oldestTaint: [sn:%llu]\n",
            clearedTaints, taints.size(),
            oldest);

    if (clearedTaints) _cpu->freeTaints();
}

template<class Impl>
void
DefaultTaintTracker<Impl>::squashFromInstSeqNum(InstSeqNum seqNum,
                                                ThreadID tid)
{
    if (taints.size() == 0) return;
    auto it = taints.begin();
    std::queue<PhysRegIdPtr> toDelete;
    std::list<int> orderedRegs;
    DPRINTF(TaintTrackerDebug, "Squashing taints, currTaints: %d\n",
            taints.size());
    while (it != taints.end()) {
        if ( it->second->seqNum > seqNum ) {
            toDelete.push(it->first);
        }
        orderedRegs.push_back(it->first->flatIndex());
        it++;
    }

    int clearedTaints = toDelete.size();
    while (toDelete.size() != 0) {
        PhysRegIdPtr elem = toDelete.front();
        taints.erase(elem);
        toDelete.pop();
        ++stats.taintsFreed;
    }
    DPRINTF(TaintTrackerDebug, "SquashedTaints: %d, "
            "currTaints: %d, [sn:%llu]\n",
            clearedTaints, taints.size(),
            seqNum);
}

template<class Impl>
DefaultTaintTracker<Impl>::
TaintTrackerStats::TaintTrackerStats(O3CPU *_cpu)
    : Stats::Group(_cpu),
    ADD_STAT(taintsInserted, UNIT_COUNT,
        "Number of taints inserted into taint tracker"),
    ADD_STAT(taintsFreed, UNIT_COUNT,
        "Number of taints freed from taint tracker")
    {

    }

#endif //__CPU_O3_TAINT_TRACKER_IMPL_HH__

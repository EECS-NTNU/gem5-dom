#ifndef __CPU_O3_TAINT_TRACKER_IMPL_HH__
#define __CPU_O3_TAINT_TRACKER_IMPL_HH__

#include "cpu/o3/taint_tracker.hh"

template<class Impl>
DefaultTaintTracker<Impl>::DefaultTaintTracker(
    O3CPU *_cpu,
    const DerivO3CPUParams &params)
    : _cpu(_cpu)
{

}


template<class Impl>
void
DefaultTaintTracker<Impl>::insertTaint(
    PhysRegIdPtr &regId, DynInstPtr &load_inst)
{
    taints.insert(std::make_pair(regId, load_inst));
}

template<class Impl>
bool
DefaultTaintTracker<Impl>::isTainted(PhysRegIdPtr regId)
{
    auto it = taints.find(regId);
    return it != taints.end();
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
    bool clearedTaints = false;
    for (auto it = taints.begin(); it != taints.end(); it++) {
        if (!it->second->underShadow()) {
            taints.erase(it->first);
            it--;
            clearedTaints = true;
        }
    }
    if (clearedTaints) _cpu->freeTaints();
}

#endif //_CPU_O3_TAINT_TRACKER_IMPL_HH__
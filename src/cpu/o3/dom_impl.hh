#ifndef __CPU_O3_DOM_IMPL_HH__
#define __CPU_O3_DOM_IMPL_HH__

#include "config/the_isa.hh"
#include "cpu/o3/dom.hh"
#include "params/DerivO3CPU.hh"

template <class Impl>
DefaultDOM<Impl>::DefaultDOM(O3CPU *_cpu, const DerivO3CPUParams &params)
    : cpu(_cpu),
    maxNumSbEntries(params.numSbEntries),
    maxNumRqEntries(params.numRqEntries),
    width(params.decodeWidth),
    numThreads(params.numThreads),
    domStats(cpu)
{
}

template <class Impl>
void
DefaultDOM<Impl>::resetState()
{
    auto numThreads = activeThreads->size();
    for (ThreadID tid = 0; tid < numThreads; tid++) {
        sbList[tid].clear();
        rqList[tid].clear();
    }
}

template <class Impl>
void
DefaultDOM<Impl>::squashThread(ThreadID tid)
{
    domStats.entriesSquashed += sbList[tid].size();
    sbList[tid].clear();
    domStats.loadsSquashed += rqList[tid].size();
    rqList[tid].clear();
}

template <class Impl>
void
DefaultDOM<Impl>::squashFromInstSeqNum(InstSeqNum seqNum, ThreadID tid)
{
    if (sbList[tid].empty()) {
        domStats.loadsSquashed += rqList[tid].size();
        rqList[tid].clear();
        return;
    }
    while (!sbList[tid].empty() &&
        std::get<0>(sbList[tid].back())->seqNum > seqNum) {
        ++domStats.entriesSquashed;
        sbList[tid].pop_back();
    }
    sbTail[tid] = (std::get<1>(sbList[tid].back()) + 1) % maxNumRqEntries;
    sbHead[tid] = (std::get<1>(sbList[tid].front()) + 1) % maxNumRqEntries;
    restoreFromIndex(tid);
}

template <class Impl>
std::string
DefaultDOM<Impl>::name() const
{
    return cpu->name()+ ".dom";
}

template<class Impl>
void
DefaultDOM<Impl>::setActiveThreads(std::list<ThreadID> *at_ptr)
{
    activeThreads = at_ptr;
}

template<class Impl>
void
DefaultDOM<Impl>::squashInstruction(const DynInstPtr &inst, ThreadID tid)
{
    if (inst->isControl()) {
        int spot = getBranchIndex(inst, tid);
        if (spot == -1) return;
        sbList[tid].erase(sbList[tid].begin() + spot);
        ++domStats.entriesSquashed;
        sbTail[tid] = (std::get<1>(sbList[tid].back()) + 1) % maxNumRqEntries;
        sbHead[tid] = (std::get<1>(sbList[tid].front()) + 1) % maxNumRqEntries;
        restoreFromIndex(tid);

    } else if (inst->isLoad()) {
        int spot = getLoadIndex(inst, tid);
        if (spot == -1) return;
        std::get<1>(rqList[tid].at(spot))->underShadow = false;
        rqList[tid].erase(rqList[tid].begin() + spot);
        ++domStats.loadsSquashed;
    }
}

template <class Impl>
void
DefaultDOM<Impl>::insertBranch(const DynInstPtr &inst, ThreadID tid)
{
    DPRINTF(DebugDOM, "Trying to insert branch\n");
    assert(inst);

    std::tuple<DynInstPtr, int, bool> tup =
        std::make_tuple(inst, sbTail[tid], true);
    sbTail[tid] = (sbTail[tid] + 1) % maxNumSbEntries;
    sbList[tid].push_back(tup);
    DPRINTF(DOM, "[tid:%i] Inserted Branch into ShadowBuffer .\n", tid);
    ++domStats.branchesInserted;
    assert(sbList[tid].size() < maxNumSbEntries);
}

template <class Impl>
void
DefaultDOM<Impl>::insertLoad(const DynInstPtr &inst, ThreadID tid)
{
    DPRINTF(DebugDOM, "Trying to insert load\n");
    assert(inst);
    if (sbHead[tid] != sbTail[tid]) {
        rqList[tid].push_back({sbTail[tid], inst});
        inst->underShadow = true;
        DPRINTF(DOM, "[tid:%i] Inserted Load into ReleaseQueue.\n", tid);
        ++domStats.loadsInserted;
    }
    assert(rqList[tid].size() < maxNumRqEntries);
}

template <class Impl>
int
DefaultDOM<Impl>::getBranchIndex(const DynInstPtr &inst, ThreadID tid)
{
    for (int index = 0; index < sbList[tid].size(); index++) {
        if (std::get<0>(sbList[tid].at(index)) == inst) {
            return index;
        }
    }
    return -1;
}

template <class Impl>
int
DefaultDOM<Impl>::getLoadIndex(const DynInstPtr &inst, ThreadID tid)
{
    for (int index = 0; index < rqList[tid].size(); index++) {
        if (std::get<1>(rqList[tid].at(index)) == inst) {
            return index;
        }
    }
    return -1;
}

template <class Impl>
void
DefaultDOM<Impl>::safeBranch(const DynInstPtr &inst, ThreadID tid)
{
    DPRINTF(DebugDOM, "Trying to safe branch\n");
    assert(inst);
    int spot = getBranchIndex(inst, tid);
    if (spot == -1) {
        DPRINTF(DOM, "[tid:%i] Branch not found to clear\n");
        return;
    }
    std::get<2>(sbList[tid].at(spot)) = false;
    DPRINTF(DOM, "[tid:%i] Declared Branch Safe.\n", tid);
    ++domStats.branchesCleared;
}

template <class Impl>
void
DefaultDOM<Impl>::mispredictBranch(const DynInstPtr &inst, ThreadID tid)
{
    DPRINTF(DebugDOM, "Trying to handle mispredicted branch\n");
    assert(inst);
    int spot = -1;
    for (int index = 0; index < sbList[tid].size(); index++) {
        if (std::get<0>(sbList[tid].at(index)) == inst) {
            spot = index;
        }
    }
    DPRINTF(DebugDOM, "Done searching, spot is %i\n", spot);
    if (spot == -1) {
        DPRINTF(DOM, "[tid:%i] Branch not found after mispredict\n", tid);
        return;
    }
    while (sbList[tid].size() > (spot + 1)) {
        sbList[tid].pop_back();
        ++domStats.entriesSquashed;
    }
    DPRINTF(DebugDOM, "Done popping off entries\n");
    std::get<2>(sbList[tid].at(spot)) = false;
    sbTail[tid] = (std::get<1>(sbList[tid].at(spot)) + 1) % maxNumRqEntries;
    DPRINTF(DOM, "[tid:%i] Restored ShadowBuffer to "
    "mispredicted instruction.\n", tid);
    ++domStats.timesMispredicted;
    restoreFromIndex(tid);
}

template <class Impl>
void
DefaultDOM<Impl>::restoreFromIndex(ThreadID tid)
{
    DPRINTF(DebugDOM, "Trying to restore rq from index\n");
    if (rqList[tid].empty()) return;
    while (!rqList[tid].empty() &&
        !tagCheck(std::get<0>(rqList[tid].back()), tid)) {
        std::get<1>(rqList[tid].back())->underShadow = false;
        rqList[tid].erase(rqList[tid].end() -1);
        ++domStats.loadsSquashed;
    }
    DPRINTF(DOM, "[tid:%i] Restored ReleaseQueue to new index.\n", tid);
}

template <class Impl>
void
DefaultDOM<Impl>::stepSb(ThreadID tid)
{
    if (sbList[tid].empty()) return;
    if (!(std::get<2>(sbList[tid].front())) && sbHead[tid] != sbTail[tid]) {
        sbHead[tid] = (sbHead[tid] + 1) % maxNumSbEntries;
        sbList[tid].erase(sbList[tid].begin());
        DPRINTF(DOM, "[tid:%i] Stepped ShadowBuffer.\n", tid);
    } else if (std::get<0>(sbList[tid].front())->isCommitted()) {
        sbHead[tid] = (sbHead[tid] + 1) % maxNumSbEntries;
        sbList[tid].erase(sbList[tid].begin());
        DPRINTF(DOM, "[tid:%i] Abnormal Branch stepped at ShadowBuffer.\n",
        tid);
        ++domStats.abnormalBranches;
    }
}

template <class Impl>
bool
DefaultDOM<Impl>::tagCheck(int sbTag, ThreadID tid)
{
    if (sbHead[tid] < sbTail[tid]) {
        return sbHead[tid] <= sbTag && sbTag < sbTail[tid];
    }
    return sbHead[tid] <= sbTag || sbTag < sbTail[tid];
}

template <class Impl>
void
DefaultDOM<Impl>::stepRq(ThreadID tid)
{
    if (rqList[tid].empty()) return;
    if (!(tagCheck(std::get<0>(rqList[tid].front()), tid))) {
        std::get<1>(rqList[tid].front())->underShadow = false;
        rqList[tid].erase(rqList[tid].begin());
        ++domStats.loadsCleared;
        DPRINTF(DOM, "[tid:%i] Stepped ReleaseQueue.\n", tid);
    } else if (std::get<1>(rqList[tid].front())->isCommitted()) {
        std::get<1>(rqList[tid].front())->underShadow = false;
        rqList[tid].erase(rqList[tid].begin());
        ++domStats.abnormalLoads;
        DPRINTF(DOM, "[tid:%i] Abnormal Load stepped at ReleaseQueue.\n", tid);
    }
}

template <class Impl>
void
DefaultDOM<Impl>::tick()
{
    for (ThreadID i = 0; i < activeThreads->size(); i++) {
        DPRINTF(DebugDOM, "Trying to step SB\n");
        for (int j = 0; j < width; j++) {
            stepSb(i);
        }
        DPRINTF(DebugDOM, "Trying to step RQ\n");
        for (int j = 0; j < width; j++) {
            stepRq(i);
        }
        if (rqList[i].size() > 0) {
            DPRINTF(DebugDOM, "Current State for thread %i: \n"
        "Entries in SB: %i\n"
        "Entries in RQ: %i\n"
        "sbTag for head of RQ: %i\n"
        "SBHead: %i, SBTail: %i\n",
        i, sbList[i].size(), rqList[i].size(),
        std::get<0>(rqList[i].front()), sbHead[i], sbTail[i]);

        } else {
            DPRINTF(DebugDOM, "Current State for thread %i: \n"
            "Entries in SB: %i\n"
            "Entries in RQ: %i\n"
            "SBHead: %i, SBTail: %i\n",
            i, sbList[i].size(), rqList[i].size(), sbHead[i], sbTail[i]);

        }
        if (sbHead[i] == sbTail[i]) {
            ++domStats.emptyCycles;
        } else {
            ++domStats.activeCycles;
        }
    }

    DPRINTF(DOM, "Ticked DOM.\n");
}

template <class Impl>
DefaultDOM<Impl>::
DOMStats::DOMStats(O3CPU *cpu)
    : Stats::Group(cpu),
    ADD_STAT(emptyCycles, UNIT_CYCLE,
        "Number of cycles where ShadowBuffer is empty"),
    ADD_STAT(activeCycles, UNIT_CYCLE,
        "Number of cycles where ShadowBuffer is not empty"),
    ADD_STAT(branchesInserted, UNIT_COUNT,
        "Number of branches inserted into ShadowBuffer"),
    ADD_STAT(branchesCleared, UNIT_COUNT,
        "Number of branches cleared from ShadowBuffer (not mispredicted)"),
    ADD_STAT(timesMispredicted, UNIT_COUNT,
        "Number of times mispredicts have been handled"),
    ADD_STAT(entriesSquashed, UNIT_COUNT,
        "Number of entries squashed due to mispredicts"),
    ADD_STAT(loadsSquashed, UNIT_COUNT,
        "Number of loads squashed due to mispredicts"),
    ADD_STAT(loadsInserted, UNIT_COUNT,
        "Number of entries inserted into ReleaseQueue"),
    ADD_STAT(loadsCleared, UNIT_COUNT,
        "Number of entries cleared from ReleaseQueue"),
    ADD_STAT(abnormalBranches, UNIT_COUNT,
        "Number of branches not cleared by misprediction or step"),
    ADD_STAT(abnormalLoads, UNIT_COUNT,
        "Number of loads not cleared by misprediction or step")
    {

    }


#endif//__CPU_O3_DOM_IMPL_HH__

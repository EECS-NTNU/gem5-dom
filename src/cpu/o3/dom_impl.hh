#ifndef __CPU_O3_DOM_IMPL_HH__
#define __CPU_O3_DOM_IMPL_HH__

#include "config/the_isa.hh"
#include "cpu/o3/dom.hh"
#include "params/DerivO3CPU.hh"

template <class Impl>
DefaultDOM<Impl>::DefaultDOM(O3CPU *_cpu, const DerivO3CPUParams &params)
    : _cpu(_cpu),
    maxNumSbEntries(params.numSbEntries),
    maxNumRqEntries(params.numRqEntries),
    width(params.commitWidth),
    numThreads(params.numThreads),
    sbList {},
    rqList {},
    domStats(_cpu)
{
    for (int i = 0; i < numThreads; i++) {
        sbList[i] = new std::vector<std::tuple<DynInstPtr, int, bool>>();
        rqList[i] = new std::vector<std::tuple<int, DynInstPtr>>();
    }
}

template <class Impl>
void
DefaultDOM<Impl>::resetState()
{
    auto numThreads = activeThreads->size();
    for (ThreadID tid = 0; tid < numThreads; tid++) {
        sbList[tid]->clear();
        rqList[tid]->clear();
    }
}

template <class Impl>
void
DefaultDOM<Impl>::squashThread(ThreadID tid)
{
    domStats.entriesSquashed += sbList[tid]->size();
    sbList[tid]->clear();
    domStats.loadsSquashed += rqList[tid]->size();
    rqList[tid]->clear();
    sbTail[tid] = sbHead[tid];
    DPRINTF(DebugDOM, "Squashed Thread %d\n", tid);
}

template <class Impl>
void
DefaultDOM<Impl>::squashFromInstSeqNum(InstSeqNum seqNum, ThreadID tid)
{
    int squashedEntries = 0;
    while (!sbList[tid]->empty() &&
        std::get<0>(sbList[tid]->back())->seqNum > seqNum) {
        ++domStats.entriesSquashed;
        squashedEntries++;
        sbList[tid]->pop_back();
    }
    if (sbList[tid]->size() > 0) {
        sbTail[tid] = (std::get<1>(sbList[tid]->back()) + 1) % maxNumSbEntries;
        sbHead[tid] = (std::get<1>(sbList[tid]->front())) % maxNumSbEntries;
    } else {
        sbTail[tid] = sbHead[tid];
    }
    DPRINTF(DebugDOM, "Squashed %d SB entries from [sn:%llu]\n",
        squashedEntries, seqNum);

    int squashedLoads = 0;
    while (!rqList[tid]->empty() &&
        std::get<1>(rqList[tid]->back())->seqNum > seqNum) {
        ++domStats.loadsSquashed;
        squashedLoads++;
        std::get<1>(rqList[tid]->back())->cShadow = false;
        rqList[tid]->pop_back();
    }

    DPRINTF(DebugDOM, "Squashed %d RQ loads from [sn:%llu]\n",
        squashedLoads, seqNum);
}

template <class Impl>
std::string
DefaultDOM<Impl>::name() const
{
    return _cpu->name()+ ".dom";
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
        // If not in our buffer, just leave
        if (spot == -1) return;
        sbList[tid]->erase(sbList[tid]->begin() + spot);
        ++domStats.entriesSquashed;
        sbTail[tid] = (std::get<1>(sbList[tid]->back()) + 1) %maxNumSbEntries;
        sbHead[tid] = (std::get<1>(sbList[tid]->front()) + 1)%maxNumSbEntries;
        DPRINTF(DebugDOM, "Squashed single ctrl inst [sn:%llu]. "
            "Restoring from index\n", inst->seqNum);
        //restoreFromIndex(tid);

    } else if (inst->isLoad()) {
        int spot = getLoadIndex(inst, tid);
        if (spot == -1) return;
        std::get<1>(rqList[tid]->at(spot))->cShadow = false;
        rqList[tid]->erase(rqList[tid]->begin() + spot);
        ++domStats.loadsSquashed;
        DPRINTF(DebugDOM, "Squashed single load [sn:%llu]. "
                "No need to restore\n",
            inst->seqNum);
    }
}

template <class Impl>
void
DefaultDOM<Impl>::insertBranch(const DynInstPtr &inst, ThreadID tid)
{
    if (!(_cpu->safeMode)) return;

    DPRINTF(DebugDOM, "Trying to insert branch\n");
    assert(inst);

    std::tuple<DynInstPtr, int, bool> tup =
        std::make_tuple(inst, sbTail[tid], true);
    sbTail[tid] = (sbTail[tid] + 1) % maxNumSbEntries;
    sbList[tid]->push_back(tup);
    DPRINTF(DOM, "[tid:%i] Inserted Branch [sn:%llu] into ShadowBuffer,"
    "size now: %d .\n", tid, inst->seqNum, sbList[tid]->size());
    ++domStats.branchesInserted;
    assert(sbList[tid]->size() < maxNumSbEntries);
}

template <class Impl>
void
DefaultDOM<Impl>::insertStore(const DynInstPtr &inst)
{
    if (!(_cpu->safeMode)) return;
    assert (inst);

    unresolvedStores.push_back(inst);
}

template <class Impl>
void
DefaultDOM<Impl>::insertLoad(const DynInstPtr &inst, ThreadID tid)
{
    if (!(_cpu->safeMode)) return;

    DPRINTF(DebugDOM, "Trying to insert load\n");
    assert(inst);
    if ((!unresolvedStores.empty() &&
        getOldestStore()->seqNum < inst->seqNum) ||
        sbHead[tid] != sbTail[tid]) {
        rqList[tid]->push_back({sbTail[tid] - 1, inst});
        inst->cShadow = true;
        inst->dShadow = true;
        DPRINTF(DOM, "[tid:%i] Inserted Load [sn:%llu] into ReleaseQueue,"
        "with tag %d, size now: %d. \n",
        tid, inst->seqNum, sbTail[tid] - 1, rqList[tid]->size());
        ++domStats.loadsInserted;
    }
    assert(rqList[tid]->size() < maxNumRqEntries);
}

template <class Impl>
int
DefaultDOM<Impl>::getBranchIndex(const DynInstPtr &inst, ThreadID tid)
{
    for (int index = 0; index < sbList[tid]->size(); index++) {
        if (std::get<0>(sbList[tid]->at(index)) == inst) {
            return index;
        }
    }
    return -1;
}

template <class Impl>
int
DefaultDOM<Impl>::getLoadIndex(const DynInstPtr &inst, ThreadID tid)
{
    for (int index = 0; index < rqList[tid]->size(); index++) {
        if (std::get<1>(rqList[tid]->at(index)) == inst) {
            return index;
        }
    }
    return -1;
}

template <class Impl>
void
DefaultDOM<Impl>::safeBranch(const DynInstPtr &inst, ThreadID tid)
{
    if (!(_cpu->safeMode)) return;

    DPRINTF(DebugDOM, "Trying to safe branch\n");
    assert(inst);
    int spot = getBranchIndex(inst, tid);
    if (spot == -1) {
        DPRINTF(DOM, "[tid:%i] Branch [sn:%llu] not found to clear\n",
        tid, inst->seqNum);
        return;
    }
    std::get<2>(sbList[tid]->at(spot)) = false;
    DPRINTF(DOM, "[tid:%i] Declared Branch [sn:%llu] Safe.\n", tid,
        inst->seqNum);
    ++domStats.branchesCleared;
}

template <class Impl>
void
DefaultDOM<Impl>::mispredictBranch(const DynInstPtr &inst, ThreadID tid)
{
    if (!(_cpu->safeMode)) return;

    DPRINTF(DebugDOM, "Trying to handle mispredicted branch\n");
    assert(inst);
    int spot = -1;
    int squashedEntries = 0;
    for (int index = 0; index < sbList[tid]->size(); index++) {
        if (std::get<0>(sbList[tid]->at(index)) == inst) {
            spot = index;
        }
    }
    DPRINTF(DebugDOM, "Done searching, spot is %i\n", spot);
    if (spot == -1) {
        DPRINTF(DOM, "[tid:%i] Branch [sn:%llu] not found after mispredict\n"
            , tid, inst->seqNum);
        return;
    }
    while (sbList[tid]->size() > (spot + 1)) {
        sbList[tid]->pop_back();
        ++domStats.entriesSquashed;
        squashedEntries++;
    }
    DPRINTF(DebugDOM, "Cleared %d entries due to mispredict\n",
        squashedEntries);
    sbTail[tid] = (std::get<1>(sbList[tid]->at(spot)) + 1) % maxNumSbEntries;
    DPRINTF(DOM, "[tid:%i] Restored ShadowBuffer to "
    "mispredicted instruction [sn:%llu].\n", tid, inst->seqNum);
    ++domStats.timesMispredicted;
    //restoreFromIndex(tid);
}

template <class Impl>
void
DefaultDOM<Impl>::handleSpecialArmCase()
{
    if (sbList[0]->empty()) return;
    DynInstPtr inst = std::get<0>(sbList[0]->front());
    if (inst->isLoad() &&
        unresolvedStores.front()->seqNum > inst->seqNum) {
        inst->cShadow = false;
        inst->dShadow = false;
    }
}

template <class Impl>
typename Impl::DynInstPtr
DefaultDOM<Impl>::getOldestStore()
{
    assert(!unresolvedStores.front()->effAddrValid());
    return unresolvedStores.front();
}

template <class Impl>
void
DefaultDOM<Impl>::restoreFromIndex(ThreadID tid)
{
    DPRINTF(DebugDOM, "Trying to restore rq from index\n");
    int squashed = 0;
    while (!rqList[tid]->empty() &&
            !(sbList[tid]->empty() && unresolvedStores.empty())) {
        DynInstPtr inst = std::get<1>(rqList[tid]->back());
        if ((sbList[tid]->empty()
            || std::get<0>(sbList[tid]->back())->seqNum > inst->seqNum)
            &&
            (unresolvedStores.empty()
            || unresolvedStores.back()->seqNum > inst->seqNum)) {
            break;
        } else {
            //assert(std::get<1>(rqList[tid]->back())->isSquashed());
            rqList[tid]->erase(rqList[tid]->end() - 1);
            ++domStats.loadsSquashed;
            squashed++;
        }
    }
    if (sbList[tid]->empty() && unresolvedStores.empty()) {
        while (!rqList[tid]->empty())
        {
            //assert(std::get<1>(rqList[tid]->back())->isSquashed());
            rqList[tid]->erase(rqList[tid]->end() - 1);
            ++domStats.loadsSquashed;
            squashed++;
        }

    }
    DPRINTF(DOM, "[tid:%i] Restored ReleaseQueue to index: %d"
        " with inst [sn:%llu] and squashing %d loads.\n", tid,
        rqList[tid]->empty() ? 0 : std::get<0>(rqList[tid]->front()),
        rqList[tid]->empty() ? 0 : std::get<1>(rqList[tid]->front())->seqNum,
        squashed);
}

template <class Impl>
void
DefaultDOM<Impl>::stepSb(ThreadID tid)
{
    if (sbList[tid]->empty()) return;
    if (!(std::get<2>(sbList[tid]->front())) && sbHead[tid] != sbTail[tid]) {
        sbHead[tid] = (sbHead[tid] + 1) % maxNumSbEntries;
        auto seqNum = std::get<0>(sbList[tid]->front())->seqNum;
        sbList[tid]->erase(sbList[tid]->begin());
        DPRINTF(DOM, "[tid:%i] Stepped ShadowBuffer. Freed [sn:%llu],"
                " New inst [sn:%llu].\n",
                tid,
                seqNum,
                sbList[tid]->empty() ? 0 :
                    std::get<0>(sbList[tid]->front())->seqNum);
        stallCycles = 0;
    }
}

template <class Impl>
void
DefaultDOM<Impl>::stepStores()
{
    for (int i = 0; i < unresolvedStores.size(); i++) {
        if (unresolvedStores.at(i)->effAddrValid()) {
            unresolvedStores.erase(unresolvedStores.begin() + i);
            i--;
        }
    }
}

template <class Impl>
bool
DefaultDOM<Impl>::tagCheck(int sbTag, ThreadID tid)
{
    if (sbList[tid]->empty()) return false;
    if (sbHead[tid] < sbTail[tid]) {
        return sbHead[tid] <= sbTag && sbTag < sbTail[tid];
    }
    return (sbHead[tid] <= sbTag || sbTag < sbTail[tid]);
}

template <class Impl>
void
DefaultDOM<Impl>::stepRq(ThreadID tid)
{
    if (rqList[tid]->empty()) return;
    if (!unresolvedStores.empty() &&
        getOldestStore()->seqNum <
            (std::get<1>(rqList[tid]->front()))->seqNum) return;
    if (!(tagCheck(std::get<0>(rqList[tid]->front()), tid))) {
        std::get<1>(rqList[tid]->front())->cShadow = false;
        std::get<1>(rqList[tid]->front())->dShadow = false;
        DPRINTF(DOM, "[tid:%i] Stepped ReleaseQueue, freeing"
                "[sn:%llu].\n",
                tid,
                (std::get<1>(rqList[tid]->front())->seqNum));
        rqList[tid]->erase(rqList[tid]->begin());
        ++domStats.loadsCleared;
        stallCycles = 0;
    }
}

template <class Impl>
void
DefaultDOM<Impl>::clearDeadEntries()
{
    for (ThreadID i = 0; i < activeThreads->size(); i++) {
        bool killed = false;
        for (int j = 0; j < sbList[i]->size(); j++) {
            DynInstPtr inst = std::get<0>(sbList[i]->at(j));
            if (inst->isSquashed() || inst->isCommitted()) {
                sbList[i]->erase(sbList[i]->begin() + j);
                j--;
                ++domStats.abnormalBranches;
                killed = true;
            }
        }
        for (int j = 0; j < rqList[i]->size(); j++) {
            DynInstPtr inst = std::get<1>(rqList[i]->at(j));
            if (inst->isSquashed() || inst->isCommitted()) {
                inst->cShadow = false;
                inst->dShadow = false;
                rqList[i]->erase(rqList[i]->begin() + j);
                j--;
                ++domStats.abnormalLoads;
            }
        }
        for (int j = 0; j < unresolvedStores.size(); j++) {
            DynInstPtr inst = unresolvedStores.at(i);
            if (inst->isSquashed() || inst->isCommitted()) {
                unresolvedStores.erase(unresolvedStores.begin() + i);
                j--;
                killed = true;
            }
        }
        if (killed) {
            if (sbList[i]->empty()) {
                sbTail[i] = sbHead[i];
            } else {
                sbTail[i] = (std::get<1>(sbList[i]->back()) + 1)
                    % maxNumSbEntries;
                sbHead[i] = (std::get<1>(sbList[i]->front()))
                    % maxNumSbEntries;
            }
            //restoreFromIndex(i);
        }
    }
}

template <class Impl>
void
DefaultDOM<Impl>::tick()
{
    if (!(_cpu->safeMode)) return;

    clearDeadEntries();
    stallCycles++;
    for (ThreadID i = 0; i < activeThreads->size(); i++) {
        DPRINTF(DebugDOM, "Trying to step SB\n");
        for (int j = 0; j < width; j++) {
            stepSb(i);
        }
        stepStores();
        DPRINTF(DebugDOM, "Trying to step RQ\n");
        for (int j = 0; j < width; j++) {
            stepRq(i);
        }
        handleSpecialArmCase();
        if (rqList[i]->size() > 0) {
            DPRINTF(DebugDOM, "Current State for thread %i: \n"
        "Entries in SB: %i, [sn:%llu] at head\n"
        "Entries in RQ: %i, [sn:%llu] at head\n"
        "sbTag for head of RQ: %i\n"
        "SBHead: %i, SBTail: %i\n",
        i,
        sbList[i]->size(),
        sbList[i]->size() > 0 ?
            std::get<0>(sbList[i]->front())->seqNum : 0,
        rqList[i]->size(),
        rqList[i]->size() > 0 ?
            std::get<1>(rqList[i]->front())->seqNum : 0,
        std::get<0>(rqList[i]->front()), sbHead[i], sbTail[i]);

        } else {
            DPRINTF(DebugDOM, "Current State for thread %i: \n"
            "Entries in SB: %i\n"
            "Entries in RQ: %i\n"
            "SBHead: %i, SBTail: %i\n",
            i, sbList[i]->size(), rqList[i]->size(), sbHead[i], sbTail[i]);

        }
        if (sbHead[i] == sbTail[i]) {
            ++domStats.emptyCycles;
        } else {
            ++domStats.activeCycles;
        }

        // Sanity checks
        if ((sbList[i]->size() == 0) && (sbHead[i] != sbTail[i])) {
            squashFromInstSeqNum(0, i);
            assert(!resetSwitch);
            resetSwitch = true;
        }
        //assert(!(sbList[i]->size() == 0 && sbHead[i] != sbTail[i]));
        //assert(!((sbHead[i] == sbTail[i]) && (sbList[i]->size() > 0)));
        assert(!(stallCycles > 10000));

        if (sbList[i]->size() > 0) {
            DynInstPtr inst = std::get<0>(sbList[i]->front());
            for (int j = 0; j < inst->numSrcRegs(); j++) {
                if (_cpu->taintTracker
                    .isTainted(inst->regs.renamedSrcIdx(j))) {
                    assert((!_cpu->taintTracker
                        .getTaintInstruction(inst->regs
                            .renamedSrcIdx(j))->cShadow)
                        || inst->seqNum >
                            (std::get<1>(rqList[i]->front()))->seqNum);
                }
            }
        }
    }

    DPRINTF(DebugDOM, "Ticked DOM.\n");
}

template <class Impl>
DefaultDOM<Impl>::
DOMStats::DOMStats(O3CPU *_cpu)
    : Stats::Group(_cpu),
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

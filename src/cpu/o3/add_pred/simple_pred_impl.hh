#ifndef __CPU_PRED_ADD_PRED_SIMPLE_PRED_CC__
#define __CPU_PRED_ADD_PRED_SIMPLE_PRED_CC__

#include <iostream>

#include "cpu/o3/add_pred/simple_pred.hh"
#include "debug/AddrPredDebug.hh"
#include "debug/AddrPrediction.hh"

template<class Impl>
SimplePred<Impl>::SimplePred(O3CPU *_cpu, const DerivO3CPUParams &params)
    : confidenceSaturation(params.confidence_saturation),
      confidenceThreshold(params.confidence_threshold),
      confidenceUpStep(params.confidence_up_step),
      confidenceDownStep(params.confidence_down_step),
      stats(_cpu)
{
    DPRINTF(AddrPredDebug, "SimplePred created with "
            "saturation: %d, threshold: %d, upStep: %d, downStep: %d\n",
            this->confidenceSaturation,
            this->confidenceThreshold,
            this->confidenceUpStep,
            this->confidenceDownStep);
}

template<class Impl>
AddrHistory*
SimplePred<Impl>::getMatchingEntry(Addr pc)
{
    //pc is unique down to LSB due to x86, no right-shift
    int index = pc % numEntries;
    auto possibleEntries = entries[index];
    for (int i = 0; i < associativity; i++) {
        if (possibleEntries[i] &&
            possibleEntries[i]->pc == pc)
            return possibleEntries[i];
    }
    return nullptr;
}

template<class Impl>
void
SimplePred<Impl>::insertNewEntry(AddrHistory *entry)
{
    int index = entry->pc % numEntries;
    auto possibleEntries = entries[index];
    int oldestIndex = 0;
    Tick oldestTick = MaxTick;
    for (int i = 0; i < associativity; i++) {
        if (!possibleEntries[i]) {
            possibleEntries[i] = entry;
            return;
        }
        if (possibleEntries[i]->lastAccess < oldestTick) {
            oldestTick = possibleEntries[i]->lastAccess;
            oldestIndex = i;
        }
    }
    delete(possibleEntries[oldestIndex]);
    possibleEntries[oldestIndex] = entry;
}

template<class Impl>
Addr
SimplePred<Impl>::predictFromPC(Addr pc, int runAhead)
{
    DPRINTF(AddrPrediction, "Attempting to predict for pc "
        "%llx with runahead %d\n", pc, runAhead);
    struct AddrHistory* entry = getMatchingEntry(pc);
    if (entry && entry->confidence >= this->confidenceThreshold) {
        return entry->lastAddr + (entry->strideHistory.front()*runAhead);
    }
    return 0;
}

template<class Impl>
void
SimplePred<Impl>::updatePredictor(Addr realAddr, Addr pc,
                                   InstSeqNum seqNum,
                                   int packetSize)
{
    DPRINTF(AddrPrediction, "Updating predictor for [sn:%llu], "
            "with pc %llx and realAddr %#x\n",
            seqNum, pc, realAddr);
    struct AddrHistory* entry = getMatchingEntry(pc);
    if (!entry) {
        AddrHistory* new_entry =
            new AddrHistory(seqNum, pc, realAddr, 1,
                            packetSize, curTick());
        insertNewEntry(new_entry);
        DPRINTF(AddrPredDebug, "New entry created, returning\n");
        return;
    }
    if (pc != entry->pc) {
        delete(entry);
        AddrHistory* new_entry =
            new AddrHistory(seqNum, pc, realAddr, 1,
                            packetSize, curTick());
        insertNewEntry(new_entry);
        DPRINTF(AddrPredDebug, "New tag for old entry, returning\n");
        return;
    }
    assert(pc == entry->pc);
    entry->lastAccess = curTick();

    if (entry->strideHistory.empty()) {
        entry->strideHistory.push_back(realAddr - entry->lastAddr);
        DPRINTF(AddrPredDebug, "Empty stridehistory, making new\n");
        entry->lastAddr = realAddr;
        assert(!entry->strideHistory.empty());
        return;
    }

    Addr strideAddr = entry->lastAddr + entry->strideHistory.front();

    if (strideAddr == 0) {
    } else if (strideAddr == realAddr) {
        entry->confidence += this->confidenceUpStep;
        if (entry->confidence > this->confidenceSaturation)
            entry->confidence = this->confidenceSaturation;
    } else {
        entry->confidence -= this->confidenceDownStep;
        if (entry->confidence < 0)
            entry->confidence = 0;
    }
    entry->strideHistory.at(0) = realAddr - entry->lastAddr;
    entry->lastAddr = realAddr;
}

template<class Impl>
int
SimplePred<Impl>::getPacketSize(Addr pc)
{
    struct AddrHistory* entry = getMatchingEntry(pc);
    if (!entry) return 0;
    return entry->packetSize;
}

template<class Impl>
const std::string
SimplePred<Impl>::name() const
{
    return "simple_predictor";
}

template<class Impl>
SimplePred<Impl>::SimplePredStats::SimplePredStats(Stats::Group *parent)
    : Stats::Group(parent)
{
}

#endif //__CPU_PRED_ADD_PRED_SIMPLE_PRED_CC__

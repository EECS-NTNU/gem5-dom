#ifndef __CPU_PRED_ADD_PRED_DELTA_PRED_CC__
#define __CPU_PRED_ADD_PRED_DELTA_PRED_CC__

#include "cpu/o3/add_pred/delta_pred.hh"

DeltaPred::DeltaPred(const DeltaPredParams &params)
    : BaseAddPred(params), stats(nullptr)
{
}

DeltaPred::~DeltaPred()
{

}

Addr
DeltaPred::predictFromPC(Addr pc, int runAhead) {
    auto it = entries.find(pc);
    if (it == entries.end()) return 0;
    struct AddrHistory* entry = it->second;
    DPRINTF(AddrPrediction, "Attempting to predict for pc %llx with "
        "runAhead %d. stride history size: %d and confidence :%d \n",
        pc, runAhead, entry->strideHistory.size(), entry->confidence);
    if (entry->strideHistory.size() < deltaHistory) {
        return 0;
    }
    DPRINTF(AddrPredDebug, "strideHistory: "
            "%d %d %d %d %d %d %d %d %d %d %d %d\n",
            entry->strideHistory[0],
            entry->strideHistory[1],
            entry->strideHistory[2],
            entry->strideHistory[3],
            entry->strideHistory[4],
            entry->strideHistory[5],
            entry->strideHistory[6],
            entry->strideHistory[7],
            entry->strideHistory[8],
            entry->strideHistory[9],
            entry->strideHistory[10],
            entry->strideHistory[11]);
    int patternSize = longestMatchingPattern(pc);
    if (patternSize == 0) return 0;
    return entry->lastAddr + walkPred(pc, patternSize, runAhead);
}

void
DeltaPred::updatePredictor(Addr realAddr, Addr pc,
                                   InstSeqNum seqNum,
                                   int packetSize)
{
    DPRINTF(AddrPrediction, "Updating predictor for [sn:%llu], "
            "with pc %llx and realAddr %#x\n",
            seqNum, pc, realAddr);

    auto it = entries.find(pc);
    if (it == entries.end()) {
        AddrHistory* new_entry =
            new AddrHistory(seqNum, pc, realAddr, deltaHistory, packetSize);
        entries.insert({pc, new_entry});
        assert(entries.find(pc) != entries.end());
        DPRINTF(AddrPredDebug, "New entry created, returning\n");
        /* if (entries.size() > numEntries) {
            entries.erase(entries.begin());
            DPRINTF(AddrPredDebug, "Deleted entry due to too many entries\n");
        } */
        return;
    }
    struct AddrHistory* entry = it->second;
/*     if (pc != entry->pc) {
        delete(entry);
        AddrHistory* new_entry =
            new AddrHistory(seqNum, pc, realAddr, deltaHistory, packetSize);
        entries[index] = new_entry;
        DPRINTF(AddrPredDebug, "New tag for old entry, returning\n");
        return;
    }
 */
    assert (pc == entry->pc);

    if (entry->strideHistory.empty()) {
        entry->strideHistory.push_back(
            realAddr - entry->lastAddr);
        entry->deltaPointer = (entry->deltaPointer + 1) % deltaHistory;
        entry->lastAddr = realAddr;
        assert(!entry->strideHistory.empty());
        return;
    }
    int patternSize = longestMatchingPattern(pc);
    if (patternSize == 0) {
        if (entry->strideHistory.size() >= deltaHistory) {
            entry->strideHistory.at(entry->deltaPointer)
                = realAddr - entry->lastAddr;
        } else {
            entry->strideHistory.push_back(realAddr - entry->lastAddr);
        }
        entry->deltaPointer = (entry->deltaPointer + 1) % deltaHistory;
        entry->lastAddr = realAddr;
        DPRINTF(AddrPredDebug, "No pattern yet detected, adding entries. "
                "strideHistory size: %d, deltaPointer: %d\n",
                entry->strideHistory.size(), entry->deltaPointer);
        if (entry->strideHistory.size() >= deltaHistory) {
            DPRINTF(AddrPredDebug, "strideHistory: "
                    "%d:%d:%d:%d:%d:%d:%d:%d:%d:%d:%d:%d\n",
                    entry->strideHistory.at(0),
                    entry->strideHistory.at(1),
                    entry->strideHistory.at(2),
                    entry->strideHistory.at(3),
                    entry->strideHistory.at(4),
                    entry->strideHistory.at(5),
                    entry->strideHistory.at(6),
                    entry->strideHistory.at(7),
                    entry->strideHistory.at(8),
                    entry->strideHistory.at(9),
                    entry->strideHistory.at(10),
                    entry->strideHistory.at(11));
        }
        return;
    }

    Addr deltaAddr = entry->lastAddr + walkPred(pc, patternSize, 1);

    if (deltaAddr == 0) {

    } else if (deltaAddr == realAddr) {
        entry->confidence += confidenceUpStep;
        if (entry->confidence > confidenceSaturation)
            entry->confidence = confidenceSaturation;
    } else {
        entry->confidence -= confidenceDownStep;
        if (entry->confidence < 0)
            entry->confidence = 0;
    }

    if (entry->strideHistory.size() >= deltaHistory) {
        entry->strideHistory.at(entry->deltaPointer)
            = realAddr - entry->lastAddr;
    } else {
        entry->strideHistory.push_back(realAddr - entry->lastAddr);
    }
    entry->deltaPointer = (entry->deltaPointer + 1) % deltaHistory;
    entry->lastAddr = realAddr;
}

int
DeltaPred::getPacketSize(Addr pc) {
    auto it = entries.find(pc);
    if (it == entries.end()) return 0;
    struct AddrHistory* entry = it->second;
    return entry->packetSize;
}

Addr
DeltaPred::walkPred(Addr pc, int patternSize, int steps) {
    int pattern[patternSize];
    struct AddrHistory* entry = entries.find(pc)->second;
    for (int i = 0; i < patternSize; i++) {
        int deltaIndex = ((entry->deltaPointer - i) + deltaHistory)
                                                    % deltaHistory;
        assert(deltaIndex >= 0);
        pattern[patternSize - (i+1)] = entry->strideHistory.at(deltaIndex);
    }
    int distance = 0;
    for (int i = 0; i < steps; i++) {
        int currIndex = i % patternSize;
        distance += pattern[currIndex];
    }
    if (patternSize == 4) {
        DPRINTF(AddrPredDebug, "Walking pred with pattern %d %d %d %d\n",
        pattern[0], pattern[1], pattern[2], pattern[3]);
    } else {
        DPRINTF(AddrPredDebug, "Walking pred with pattern %d %d %d\n",
        pattern[0], pattern[1], pattern[2]);
    }
    DPRINTF(AddrPredDebug, "Finished walking for pc %llx with "
            "patternSize %d, steps %d and distance %d\n",
            pc, patternSize, steps, distance);

    return distance;
}

int
DeltaPred::longestMatchingPattern(Addr pc) {
    bool threeMatch = matchSize(pc, 3);
    bool fourMatch = matchSize(pc, 4);
    if (fourMatch)
        return 4;
    if (threeMatch)
        return 3;
    return 0;
}

bool
DeltaPred::matchSize(Addr pc, int size) {
    int pattern[size];
    struct AddrHistory* entry = entries.find(pc)->second;
    if (entry->strideHistory.size() < deltaHistory)
        return false;
    for (int i = 0; i < size; i++) {
        int deltaIndex = ((entry->deltaPointer- i) + deltaHistory)
                                                   % deltaHistory;
        assert(deltaIndex >= 0);
        pattern[i] = entry->strideHistory.at(deltaIndex);
    }
    bool match = true;
    for (int i = size; i < deltaHistory; i += size) {
        for (int j = 0; j < size; j++) {
        int deltaIndex = ((entry->deltaPointer- i) + deltaHistory)
                                                   % deltaHistory;
            assert(deltaIndex >= 0);
            match = match && (pattern[j] ==
                    entry->strideHistory.at(deltaIndex));
        }
    }
    if (match) {
        bool complex = false;
        for (int i = 0; i < size; i++) {
            for (int j = 0; j < size; j++) {
                if (pattern[i] != pattern[j])
                    complex = true;
            }
        }
        if (complex)
            ++stats.complexPatterns;
    }
    return match;
}
const std::string
DeltaPred::name() const
{
    return "delta_predictor";
}

DeltaPred::DeltaPredStats::DeltaPredStats(Stats::Group *parent)
    : Stats::Group(parent),
        ADD_STAT(complexPatterns, UNIT_COUNT,
                "Number of complex patterns detected")
{
}

#endif //__CPU_PRED_ADD_PRED_DELTA_PRED_CC__
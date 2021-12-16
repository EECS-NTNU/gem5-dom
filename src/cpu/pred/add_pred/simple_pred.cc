#include "cpu/pred/add_pred/simple_pred.hh"

template <class Impl>
Addr
BaseAddPred<Impl>::predictFromPC(Addr pc, int runAhead)
{
    int index = pc % numEntries;
    AddrHistory* entry = entries[index];
    if (entry->confidence >= 8) {
        return entry->lastAddr + (entry->strideHistory[0]*runAhead);
    }
    return 0;
}

template<class Impl>
void
BaseAddPred<Impl>::updatePredictor(Addr pc, Addr predAddr,
                                  Addr realAddr)
{
    int index = pc % numEntries;
    AddrHistory* entry = entries[index];
    if (predAddr == 0) {

    } else if (predAddr == realAddr) {
        entry->confidence++;
    } else {
        entry->confidence--;
    }
    entry->strideHistory.pop()
    entry->strideHistory.push(
                              realAddr - entry->lastAddr);
    entry->lastAddr = realAddr;
}

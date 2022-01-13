#ifndef __CPU_PRED_ADD_PRED_DELTA_PRED_CC__
#define __CPU_PRED_ADD_PRED_DELTA_PRED_CC__
#include "cpu/o3/add_pred/delta_pred.hh"

Addr
BaseAddPred::predictFromPC(Addr pc, int runAhead) {
    panic("Not implemented");
}

void
BaseAddPred::updatePredictor(Addr pc, Addr predAddr,
                                   Addr realAddr)
{
    int index = pc % numEntries;
    AddrHistory* entry = entries[index];
    panic("Not implemented");
}

#endif
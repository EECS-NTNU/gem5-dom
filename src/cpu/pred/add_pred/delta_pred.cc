#include "cpu/pred/add_pred/delta_pred.hh"

template<class Impl>
Addr
BaseAddPred<Impl>::predictFromPC(Addr pc, int runAhead) {
    panic("Not implemented");
}

template<class Impl>
void
BaseAddPred<Impl>::updatePredictor(Addr pc, Addr predAddr,
                                   Addr realAddr)
{
    int index = pc % numEntries;
    AddrHistory* entry = entries[index];
    panic("Not implemented");
}

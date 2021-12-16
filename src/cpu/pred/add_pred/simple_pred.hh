#ifndef __CPU_PRED_ADD_PRED_SIMPLE_PRED_HH__
#define __CPU_PRED_ADD_PRED_SIMPLE_PRED_HH__

#include "cpu/pred/add_pred/base_add_pred.hh"

template<class Impl>
class SimplePred : public BaseAddPred
{
    SimplePred();

    int numEntries = 1024;

    struct AddrHistory entries[numEntries];
}

#endif // __CPU_PRED_ADD_PRED_SIMPLE_PRED_HH__
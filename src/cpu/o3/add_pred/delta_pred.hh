#ifndef __CPU_PRED_ADD_PRED_DELTA_PRED_HH__
#define __CPU_PRED_ADD_PRED_DELTA_PRED_HH__

#include "cpu/o3/add_pred/base_add_pred.hh"

template<class Impl>
class DeltaPred : public BaseAddPred
{
    DeltaPred(int histSize) : deltaHistory(histSize);

    int numEntries = 1024;

    int deltaHistory;

    struct AddrHistory entries[numEntries];
}

#endif //__CPU_PRED_ADD_PRED_DELTA_PRED_HH__
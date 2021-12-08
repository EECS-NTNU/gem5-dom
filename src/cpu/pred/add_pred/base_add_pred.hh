#ifndef __CPU_PRED_ADD_PRED_BASE_ADD_PRED_HH__
#define __CPU_PRED_ADD_PRED_BASE_ADD_PRED_HH__

#include "base/statistics.hh"
#include "base/types.hh"
#include "sim/sim_object.hh"

template<class Impl>
class BaseAddPred : public SimObject
{
  public:
    BaseAddPred();

    virtual Addr predictFromPC(Addr pc, int runAhead) = 0;

    virtual void updatePredictor(Addr pc, Addr predAddr,
                                 Addr realAddr) = 0;

    struct AddrHistory {

        AddrHistory(const InstSeqNum &seq_num,
                    Addr instPC,
                    Addr lastAddr,
                    int history_size)
            : seqNum(seq_num), pc(instPC),
              lastAddr(lastAddr),
              historySize(history_size),
              confidence(0)
        {}

        InstSeqNum seqNum;

        Addr pc;

        Addr lastAddr;

        int confidence;

        int historySize;

        std::queue<int> strideHistory;
    };


}

#endif //__CPU_PRED_ADD_PRED_BASE_ADD_PRED_HH__

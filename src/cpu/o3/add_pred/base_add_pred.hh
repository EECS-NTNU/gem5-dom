#ifndef __CPU_PRED_ADD_PRED_BASE_ADD_PRED_HH__
#define __CPU_PRED_ADD_PRED_BASE_ADD_PRED_HH__

#include <queue>

#include "base/statistics.hh"
#include "base/trace.hh"
#include "base/types.hh"
#include "cpu/inst_seq.hh"
#include "sim/sim_object.hh"

struct AddrHistory {

    AddrHistory(const InstSeqNum &seq_num,
                Addr instPC,
                Addr lastAddr,
                int history_size,
                int packetSize)
        : seqNum(seq_num), pc(instPC),
          lastAddr(lastAddr),
          packetSize(packetSize),
          confidence(0),
          historySize(history_size),
          strideHistory(std::queue<int>())
    {}

    InstSeqNum seqNum;

    Addr pc;

    Addr lastAddr;

    int packetSize;

    int confidence;

    int historySize;

    std::queue<int> strideHistory;
};

class BaseAddPred : public SimObject
{
  public:
    BaseAddPred(const Params &params) :
      SimObject(params),
      confidenceSaturation(0),
      confidenceThreshold(0),
      confidenceUpStep(0),
      confidenceDownStep(0) {};

    int confidenceSaturation;

    int confidenceThreshold;

    int confidenceUpStep;

    int confidenceDownStep;

    virtual ~BaseAddPred() {};

    virtual Addr predictFromPC(Addr pc, int runAhead) = 0;

    virtual void updatePredictor(Addr realAddr, Addr pc,
                                 InstSeqNum seqNum, int packetSize)
                                 = 0;
    virtual int getPacketSize(Addr pc) = 0;
};

#endif //__CPU_PRED_ADD_PRED_BASE_ADD_PRED_HH__

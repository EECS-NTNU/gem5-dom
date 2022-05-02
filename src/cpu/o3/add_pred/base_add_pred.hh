#ifndef __CPU_PRED_ADD_PRED_BASE_ADD_PRED_HH__
#define __CPU_PRED_ADD_PRED_BASE_ADD_PRED_HH__

#include <queue>

#include "arch/registers.hh"
#include "arch/types.hh"
#include "base/statistics.hh"
#include "base/stats/group.hh"
#include "base/trace.hh"
#include "base/types.hh"
#include "config/the_isa.hh"
#include "cpu/inst_seq.hh"
#include "params/DerivO3CPU.hh"
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
          strideHistory(std::vector<int>())
    {}

    InstSeqNum seqNum;

    Addr pc;

    Addr lastAddr;

    int packetSize;

    int confidence;

    int historySize;

    int deltaPointer = 0;

    std::vector<int> strideHistory;
};

struct DerivO3CPUParams;

template<class Impl>
class BaseAddPred
{
  private:
    typedef typename Impl::CPUPol CPUPol;
    typedef typename Impl::DynInstPtr DynInstPtr;
    typedef typename Impl::O3CPU O3CPU;

    O3CPU *_cpu;

  public:
    BaseAddPred(O3CPU *_cpu, const DerivO3CPUParams &params);

    int confidenceSaturation;

    int confidenceThreshold;

    int confidenceUpStep;

    int confidenceDownStep;

    virtual Addr predictFromPC(Addr pc, int runAhead) = 0;

    virtual void updatePredictor(Addr realAddr, Addr pc,
                                 InstSeqNum seqNum, int packetSize)
                                 = 0;

    virtual int getPacketSize(Addr pc) = 0;

    struct BaseAddPredStats : public Stats::Group{

      BaseAddPredStats(Stats::Group *parent);
    } stats;

    virtual Stats::Group* getStatGroup() { return &stats; }

};

#endif //__CPU_PRED_ADD_PRED_BASE_ADD_PRED_HH__

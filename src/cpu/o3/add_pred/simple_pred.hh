#ifndef __CPU_PRED_ADD_PRED_SIMPLE_PRED_HH__
#define __CPU_PRED_ADD_PRED_SIMPLE_PRED_HH__

#include "arch/registers.hh"
#include "arch/types.hh"
#include "base/statistics.hh"
#include "base/stats/group.hh"
#include "base/types.hh"
#include "config/the_isa.hh"
#include "cpu/inst_seq.hh"
#include "params/DerivO3CPU.hh"
#include "sim/sim_object.hh"

struct DerivO3CPUParams;

struct AddrHistory {

    AddrHistory(const InstSeqNum &seq_num,
                Addr instPC,
                Addr lastAddr,
                int history_size,
                int packetSize,
                Tick lastTick)
        : seqNum(seq_num), pc(instPC),
          lastAddr(lastAddr),
          lastAccess(lastTick),
          packetSize(packetSize),
          confidence(0),
          historySize(history_size),
          strideHistory(std::vector<int>())
    {}

    InstSeqNum seqNum;

    Addr pc;

    Addr lastAddr;

    Tick lastAccess;

    int packetSize;

    int confidence;

    int historySize;

    int deltaPointer = 0;

    std::vector<int> strideHistory;
};

template<class Impl>
class SimplePred
{
    private:
        typedef typename Impl::CPUPol CPUPol;
        typedef typename Impl::DynInstPtr DynInstPtr;
        typedef typename Impl::O3CPU O3CPU;

  public:

    SimplePred(O3CPU *_cpu, const DerivO3CPUParams &params);

    int confidenceSaturation;

    int confidenceThreshold;

    int confidenceUpStep;

    int confidenceDownStep;

    AddrHistory* getMatchingEntry(Addr pc);

    void insertNewEntry(AddrHistory *entry);

    Addr predictFromPC(Addr pc, int runAhead);

    void updatePredictor(Addr realAddr, Addr pc,
                         InstSeqNum seqNum, int packetSize);

    int getPacketSize(Addr pc);

    const static int numEntries = 256;

    const static int associativity = 4;

    struct AddrHistory* entries[numEntries][associativity] = {};

    const std::string name() const;

  struct SimplePredStats : public Stats::Group{
    SimplePredStats(Stats::Group *parent);
  } stats;

    Stats::Group* getStatGroup() { return &stats; }

};

#endif // __CPU_PRED_ADD_PRED_SIMPLE_PRED_HH__

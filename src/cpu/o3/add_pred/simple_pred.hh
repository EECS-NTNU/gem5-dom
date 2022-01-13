#ifndef __CPU_PRED_ADD_PRED_SIMPLE_PRED_HH__
#define __CPU_PRED_ADD_PRED_SIMPLE_PRED_HH__

#include "base/flags.hh"
#include "base/types.hh"
#include "cpu/o3/add_pred/base_add_pred.hh"
#include "params/DerivO3CPU.hh"

class SimplePred : virtual public BaseAddPred
{
  public:

    SimplePred(const Params &params);

    virtual ~SimplePred();

    Addr predictFromPC(Addr pc, int runAhead);

    void updatePredictor(Addr realAddr, Addr pc,
                         InstSeqNum seqNum, int packetSize);

    int getPacketSize(Addr pc);

    int numEntries = 1024;

    struct AddrHistory* entries[1024];

    const std::string name() const;

};

#endif // __CPU_PRED_ADD_PRED_SIMPLE_PRED_HH__
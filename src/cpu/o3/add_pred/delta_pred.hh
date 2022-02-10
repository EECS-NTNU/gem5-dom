#ifndef __CPU_PRED_ADD_PRED_DELTA_PRED_HH__
#define __CPU_PRED_ADD_PRED_DELTA_PRED_HH__

#include "base/flags.hh"
#include "base/types.hh"
#include "cpu/o3/add_pred/base_add_pred.hh"
#include "debug/AddrPredDebug.hh"
#include "debug/AddrPrediction.hh"
#include "params/DerivO3CPU.hh"

class DeltaPred : virtual public BaseAddPred
{
    public:

        DeltaPred(const Params &params);

        virtual ~DeltaPred();

        Addr predictFromPC(Addr pc, int runAhead);

        void updatePredictor(Addr realAddr, Addr pc,
                            InstSeqNum seqNum, int packetSize);

        int getPacketSize(Addr pc);

        Addr walkPred(Addr pc, int patternSize, int steps);

        int longestMatchingPattern(Addr pc);

        bool matchSize(Addr pc, int size);

        int numEntries = 1024;

        int deltaHistory = 12;

        std::map<Addr, struct AddrHistory*> entries {};

        const std::string name() const;
};

#endif //__CPU_PRED_ADD_PRED_DELTA_PRED_HH__

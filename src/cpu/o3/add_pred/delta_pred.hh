#ifndef __CPU_PRED_ADD_PRED_DELTA_PRED_HH__
#define __CPU_PRED_ADD_PRED_DELTA_PRED_HH__

#include "base/flags.hh"
#include "base/types.hh"
#include "cpu/o3/add_pred/base_add_pred.hh"
#include "debug/AddrPredDebug.hh"
#include "debug/AddrPrediction.hh"

template<class Impl>
class DeltaPred : virtual public BaseAddPred<Impl>
{
    private:
        typedef typename Impl::CPUPol CPUPol;
        typedef typename Impl::DynInstPtr DynInstPtr;
        typedef typename Impl::O3CPU O3CPU;

    public:

        DeltaPred(O3CPU *_cpu, const DerivO3CPUParams &params);

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

    struct DeltaPredStats : public Stats::Group{
        DeltaPredStats(Stats::Group *parent);

        Stats::Scalar complexPatterns;

    } stats;

        Stats::Group* getStatGroup() { return &stats; }
};

#endif //__CPU_PRED_ADD_PRED_DELTA_PRED_HH__

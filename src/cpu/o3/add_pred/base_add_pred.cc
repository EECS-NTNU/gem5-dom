#ifndef __CPU_PRED_ADD_PRED_BASE_ADD_PRED_CC__
#define __CPU_PRED_ADD_PRED_BASE_ADD_PRED_CC__

#include "cpu/o3/add_pred/base_add_pred.hh"

BaseAddPred::BaseAddPred(const BaseAddPredParams &params)
    : SimObject(params),
      confidenceSaturation(params.confidence_saturation),
      confidenceThreshold(params.confidence_threshold),
      confidenceUpStep(params.confidence_up_step),
      confidenceDownStep(params.confidence_down_step)
{

}

BaseAddPred::~BaseAddPred()
{

}

Addr
BaseAddPred::predictFromPC(Addr pc, int runAhead)
{
    return 0;
}

void
BaseAddPred::updatePredictor(Addr realAddr, Addr pc,
                             InstSeqNum seqNum,
                             int packetSize)
{

}

int
BaseAddPred::getPacketSize(Addr pc)
{
    return 0;
}

Stats::Group*
BaseAddPred::getStatGroup()
{
    return nullptr;
}

#endif //__CPU_PRED_ADD_PRED_BASE_ADD_PRED_CC__
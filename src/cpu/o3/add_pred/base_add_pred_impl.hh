#ifndef __CPU_PRED_ADD_PRED_BASE_ADD_PRED_CC__
#define __CPU_PRED_ADD_PRED_BASE_ADD_PRED_CC__

#include "cpu/o3/add_pred/base_add_pred.hh"

template<class Impl>
BaseAddPred<Impl>::BaseAddPred(O3CPU *_cpu, const DerivO3CPUParams &params)
    : stats(_cpu),
      confidenceSaturation(params.confidence_saturation),
      confidenceThreshold(params.confidence_threshold),
      confidenceUpStep(params.confidence_up_step),
      confidenceDownStep(params.confidence_down_step),
      _cpu(_cpu)
{
}

template<class Impl>
BaseAddPred<Impl>::BaseAddPredStats::BaseAddPredStats(Stats::Group *parent)
    : Stats::Group(parent)
{
}

#endif //__CPU_PRED_ADD_PRED_BASE_ADD_PRED_CC__
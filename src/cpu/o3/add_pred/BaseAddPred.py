from m5.defines import buildEnv
from m5.params import *
from m5.proxy import *

from m5.objects.BaseCPU import BaseCPU
from m5.objects.FUPool import *
from m5.objects.O3Checker import O3Checker
from m5.objects.BranchPredictor import *
from m5.SimObject import SimObject

class BaseAddPred(SimObject):
    type = 'BaseAddPred'
    cxx_header='cpu/o3/add_pred/base_add_pred.hh'

    confidence_saturation = Param.Int(10,
                            "Maximum confidence achievable for prediction")
    confidence_threshold = Param.Int(8,
                            "Threshold to begin prediction at")
    confidence_up_step = Param.Int(1,
                            "Gain of confidence on correct prediction")
    confidence_down_step = Param.Int(4,
                            "Loss of confidence on wrong prediction")

class SimplePred(BaseAddPred):
    type = 'SimplePred'
    cxx_header='cpu/o3/add_pred/simple_pred.hh'

class DeltaPred(BaseAddPred):
    type = 'DeltaPred'
    cxx_header='cpu/o3/add_pred/delta_pred.hh'
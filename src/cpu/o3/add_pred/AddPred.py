from m5.defines import buildEnv
from m5.params import *
from m5.proxy import *

from m5.objects.BaseCPU import BaseCPU
from m5.objects.FUPool import *
from m5.objects.O3Checker import O3Checker
from m5.objects.BranchPredictor import *

class AddPred():
    type = 'AddPred'
    cxx_header='cpu/o3/add_pred/base_add_pred.hh'
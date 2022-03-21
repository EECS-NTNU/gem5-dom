from multiprocessing import Process
import os
import time
import shutil
import sys

stats = ["simTicks",
    "simTicks",
    "simOps",
    "system.switch_cpus.numCycles",
    "system.switch_cpus.numLoadInsts",
    "system.switch_cpus.emptyCycles",
    "system.switch_cpus.activeCycles",
    "system.switch_cpus.branchesInserted",
    "system.switch_cpus.branchesCleared",
    "system.switch_cpus.timesMispredicted",
    "system.switch_cpus.entriesSquashed",
    "system.switch_cpus.loadsSquashed",
    "system.switch_cpus.loadsInserted",
    "system.switch_cpus.loadsCleared",
    "system.switch_cpus.abnormalBranches",
    "system.switch_cpus.abnormalLoads",
    "system.switch_cpus.timesIdled",
    "system.switch_cpus.idleCycles",
    "system.switch_cpus.delayedLoads",
    "system.switch_cpus.squashedDelayedLoads",
    "system.switch_cpus.reissuedDelayedLoads",
    "system.switch_cpus.lsq0.loadsDelayedOnMiss"]

work_root = os.getcwd()
gem5_root=f"{work_root}"
gem5=f"{gem5_root}/build/X86/gem5.opt"
se=f"{gem5_root}/configs/example/se.py"
spec_root=f"{gem5_root}/dom/x86-spec-static-ref"
stats="m5out/stats.txt"
results=f"{gem5_root}/results"

runCPU="--cpu-type=DerivO3CPU"
memory="--mem-size=8GB"
prefetcher="--l1d-hwp-type=StridePrefetcher --l2-hwp-type=StridePrefetcher"
caches="--caches --l1d_size=32768 --l1i_size=32768 --l2_size=2097152 "\
"--l3_size=16MB --l1d_assoc=4 --l1i_assoc=4 "\
"--l2_assoc=8 --l3_assoc=16 --cacheline_size=64"

mp_args="--confidence_saturation=10 "\
"--confidence_threshold=8 --confidence_up_step=1 "\
"--confidence_down_step=4"

fast_forward="--fast-forward 1000000000"
runtime="--maxinsts=1000000000"

print(sys.argv)

cmd = sys.argv[1]
bname = sys.argv[2]
options = sys.argv[3]
fullname = sys.argv[4]
iteration = sys.argv[5]
jobid = sys.argv[6]

redirect=f"-r --stdout-file={bname}_{iteration}.simout"

def copy_dir():
    src = f'{spec_root}/{fullname}'
    dst = f'{jobid}/{bname}_{iteration}'
    shutil.copytree(src, dst)

def cleanup():
    tgt = f'{jobid}/{bname}_{iteration}'
    shutil.rmtree(tgt)

def move_result():
    src = f'{jobid}/{bname}_{iteration}/m5out/stats.txt'
    dst = f'{jobid}/results/{bname}_{iteration}.txt'
    shutil.copy(src, dst)
    src = f'{jobid}/{bname}_{iteration}/m5out/{bname}_{iteration}.simout'
    dst = f'{jobid}/results/{bname}_{iteration}.simout'
    shutil.copy(src, dst)
    src = f'{jobid}/{bname}_{iteration}/m5out/config.ini'
    dst = f'{jobid}/results/{bname}_{iteration}.config'
    shutil.copy(src, dst)

def run_benchmark():
    copy_dir()
    os.chdir(f"{jobid}/{bname}_{iteration}")
    run_ref = f"{gem5} {redirect} {se} {fast_forward} {memory} "\
        f"{runtime} {mp_args} {prefetcher} {caches} "\
        f"{runCPU} -c {bname} -o \"{options}\""

    print(f"Finished with code {os.system(run_ref)}")
    os.chdir(f'{gem5_root}')
    move_result()
    cleanup()

run_benchmark()

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
caches="--caches --l1d_size=32768 --l1i_size=32768 --l2_size=2097152 "\
"--l3_size=16MB --l1d_assoc=4 --l1i_assoc=4 "\
"--l2_assoc=8 --l3_assoc=16 --cacheline_size=64"

fast_forward="--fast-forward 1000000000"
runtime="--maxinsts=1000000000"

mkdir = f"mkdir {gem5_root}/results"

print(sys.argv)

cmd = sys.argv[1]
bname = sys.argv[2]
options = sys.argv[3]
fullname = sys.argv[4]
iteration = sys.argv[5]

def copy_dir():
    src = f'{spec_root}/{fullname}'
    dst = f'{bname}_{iteration}'
    shutil.copytree(src, dst)

def cleanup():
    tgt = f'{bname}_{iteration}'
    shutil.rmtree(tgt)

def move_result():
    src = f'{bname}_{iteration}/m5out/stats.txt'
    dst = f'results/{bname}_{iteration}.txt'
    shutil.copy(src, dst)

def run_benchmark():
    copy_dir()
    os.chdir(f"{bname}_{iteration}")
    run_ref = f"{gem5} {se} {fast_forward} {memory} "\
        f"{runtime} {caches} {runCPU} -c {bname} -o \"{options}\""

    os.system(f"Finished with code {os.system(run_ref)}")
    os.chdir(f'{gem5_root}')
    move_result()
    cleanup()

run_benchmark()

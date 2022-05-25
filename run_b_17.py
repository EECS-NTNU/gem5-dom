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
gem5=f"{gem5_root}/build/X86/gem5.fast"
se=f"{gem5_root}/configs/example/fs.py"
spec_root=f"{gem5_root}/dom/static-17"
stats="m5out/stats.txt"
results=f"{gem5_root}/results"

print(sys.argv)

index = int(sys.argv[1])
config_file = sys.argv[2]
name = sys.argv[3]

bname = ""
fullname = ""
iteration = ""


with open(f"{spec_root}/fullnames.txt") as names, \
    open(f"{spec_root}/iterations.txt") as it:
    all_names = names.readlines()
    fullname = all_names[index][:-1]
    bname = all_names[index].split(".")[1][:-1]
    iteration = it.readlines()[index][:-1]

fs=f"--checkpoint-dir {bname}-cpt --disk-image " \
   f"{gem5_root}/../fs/spec17.img --kernel {gem5_root}/../fs/plinux "\
   f"--script {gem5_root}/run_scripts/{bname}_{iteration}.rcS"
ckpt = "--fast-forward 9500000000 --at-instruction "\
       "--take-checkpoint 10000000000 --cpu-type DerivO3CPU"

options = ""

with open(f"{spec_root}/commands.txt") as commands:
    options = commands.readlines()[index].split(" ", 1)[1][:-1]

redirect=f"-r --stdout-file={bname}_{iteration}.simout"

def get_config():
    config = ""
    with open(f"{gem5_root}/run_configs/{config_file}.cfg") as cfg:
        lines = cfg.readlines()
        for line in lines:
            if line[0] == "#":
                continue
            config += f"--{line[:-1]} "
    return config

def copy_dir():
    src = f'{spec_root}/{fullname}'
    dst = f'{name}/{bname}_{iteration}'
    shutil.copytree(src, dst)

def cleanup():
    tgt = f'{name}/{bname}_{iteration}'
    shutil.rmtree(tgt)

def move_result():
    src = f'{name}/{bname}_{iteration}/m5out/stats.txt'
    dst = f'{name}/results/{bname}_{iteration}.txt'
    shutil.copy(src, dst)
    src = f'{name}/{bname}_{iteration}/m5out/{bname}_{iteration}.simout'
    dst = f'{name}/results/{bname}_{iteration}.simout'
    shutil.copy(src, dst)
    src = f'{name}/{bname}_{iteration}/m5out/config.ini'
    dst = f'{name}/results/{bname}_{iteration}.config'
    shutil.copy(src, dst)
    src = f'{name}/{bname}_{iteration}/m5out/cpt*'
    dst = f'{name}/results/cpt_1'
    shutil.copytree(src, dst)

def run_benchmark():
    copy_dir()
    os.chdir(f"{name}/{bname}_{iteration}")
    config = get_config()
    run_ref = f"{gem5} {redirect} {se} {fs} {config} {ckpt}"
    print(run_ref)
    print(f"Finished with code {os.system(run_ref)}")
    os.chdir(f'{gem5_root}')
    move_result()
    cleanup()

run_benchmark()

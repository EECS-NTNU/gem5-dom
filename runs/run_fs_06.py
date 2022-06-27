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
gem5=f"{gem5_root}/build/ARM/gem5.fast"
se=f"{gem5_root}/configs/example/fs.py"
spec_root=f"{gem5_root}/dom/arm-static"
stats="m5out/stats.txt"
results=f"{gem5_root}/results"

print(sys.argv)

index = int(sys.argv[1])
config_file = sys.argv[2]
name = sys.argv[3]

bname = ""
fullname = ""
iteration = ""


with open(f"{spec_root}/fullnames_06.txt") as names, \
    open(f"{spec_root}/iterations_06.txt") as it:
    all_names = names.readlines()
    fullname = all_names[index][:-1]
    bname = all_names[index].split(".")[1][:-1]
    iteration = it.readlines()[index][:-1]

fs=f"--checkpoint-dir {bname}_{iteration}-cpt --disk-image " \
   f"{gem5_root}/../fs/ubuntu-18-arm.img " \
   f"--kernel {gem5_root}/../fs/vlinux-arm "\
   f"--script {gem5_root}/run_scripts/spec2006/{bname}_{iteration}.rcS "\
   f"--bootloader={gem5_root}/../fs/boot-arm-2"
#ckpt = "--at-instruction --maxinsts=11000000000 "\
#       "--take-checkpoint=1000000000 --cpu-type=DerivO3CPU"
ckpt = "-r 1 --cpu-type DerivO3CPU --restore-with-cpu DerivO3CPU"

options = ""

with open(f"{spec_root}/commands_s06.txt") as commands:
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

def setup_dir():
    dst = f'{results}/{name}/{bname}_{iteration}'
    os.mkdir(dst)

def cleanup():
    tgt = f'{results}/{name}/{bname}_{iteration}'
    shutil.rmtree(tgt)


def copy_cpt():
    if (os.path.isdir(f"{bname}_{iteration}-cpt")):
        shutil.rmtree(f"{bname}_{iteration}-cpt")
    src = f"{gem5_root}/runs/cpts/spec2006/{bname}_{iteration}-cpt"
    dst = f"{bname}_{iteration}-cpt"
    shutil.copytree(src, dst)
    src_2 = f"{dst}/cpt.None.1000000000"
    dst_2 = f"{dst}/cpt.1"
    shutil.copytree(src_2, dst_2)

def scrub_switch_cpu():
    with open(f"{bname}_{iteration}-cpt/cpt.1/m5.cpt", "r+") as cpt:
        data = cpt.read().replace("switch_cpus", "cpu")
        cpt.seek(0)
        cpt.write(data)
        cpt.truncate()

def setup_cpt():
    if (os.path.isdir(f"{bname}_{iteration}-cpt")):
        shutil.rmtree(f"{bname}_{iteration}-cpt")
    os.mkdir(f"{bname}_{iteration}-cpt")

def move_result():
    src = f'{results}/{name}/{bname}_{iteration}/m5out/stats.txt'
    dst = f'{results}/{name}/results/{bname}_{iteration}.txt'
    shutil.copy(src, dst)
    src = \
    f'{results}/{name}/{bname}_{iteration}/m5out/{bname}_{iteration}.simout'
    dst = f'{results}/{name}/results/{bname}_{iteration}.simout'
    shutil.copy(src, dst)
    src = f'{results}/{name}/{bname}_{iteration}/m5out/system.terminal'
    dst = f'{results}/{name}/results/{bname}_{iteration}.stdout'
    shutil.copy(src, dst)
    src = f'{results}/{name}/{bname}_{iteration}/m5out/config.ini'
    dst = f'{results}/{name}/results/{bname}_{iteration}.config'
    shutil.copy(src, dst)
    src = f'{results}/{name}/{bname}_{iteration}/{bname}_{iteration}-cpt'
    dst = f'runs/cpts/spec2006/{bname}_{iteration}-cpt'
    shutil.copytree(src, dst)

def run_benchmark():
    setup_dir()
    os.chdir(f"{results}/{name}/{bname}_{iteration}")
    config = get_config()
    #setup_cpt()
    copy_cpt()
    scrub_switch_cpu()
    run_ref = f"{gem5} {redirect} {se} {fs} {config} {ckpt}"
    print(run_ref)
    print(f"Finished with code {os.system(run_ref)}")
    os.chdir(f'{gem5_root}')
    move_result()
    cleanup()

run_benchmark()

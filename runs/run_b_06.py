from multiprocessing import Process
import os
import time
import shutil
import sys

work_root = os.getcwd()
gem5_root=f"{work_root}"
gem5=f"{gem5_root}/build/X86/gem5.fast"
se=f"{gem5_root}/configs/example/se.py"
spec_root=f"{gem5_root}/dom/x86-spec-static-ref"
stats="m5out/stats.txt"
results=f"{gem5_root}/results"

print(sys.argv)

index = int(sys.argv[1])
config_file = sys.argv[2]
name = sys.argv[3]

bname = ""
fullname = ""
iteration = ""

failed_benchmarks= [
"gobmk_0",
"gobmk_1",
"gobmk_2",
"gobmk_3",
"gobmk_4",
"gobmk_5",
"leslie3d_0",
"tonto_0"
]


with open(f"{spec_root}/fullnames_06.txt") as names, \
    open(f"{spec_root}/iterations_06.txt") as it:
    all_names = names.readlines()
    fullname = all_names[index][:-1]
    bname = all_names[index].split(".")[1][:-1]
    iteration = it.readlines()[index][:-1]

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

def copy_dir():
    src = f'{spec_root}/{fullname}'
    dst = f'{results}/{name}/{bname}_{iteration}'
    shutil.copytree(src, dst)

def cleanup():
    tgt = f'{results}/{name}/{bname}_{iteration}'
    shutil.rmtree(tgt)

def move_result():
    src = f'{results}/{name}/{bname}_{iteration}/m5out/stats.txt'
    dst = f'{results}/{name}/results/{bname}_{iteration}.txt'
    shutil.copy(src, dst)
    src = \
    f'{results}/{name}/{bname}_{iteration}/m5out/{bname}_{iteration}.simout'
    dst = f'{results}/{name}/results/{bname}_{iteration}.simout'
    shutil.copy(src, dst)
    src = f'{results}/{name}/{bname}_{iteration}/m5out/config.ini'
    dst = f'{results}/{name}/results/{bname}_{iteration}.config'
    shutil.copy(src, dst)

def run_benchmark():
    if f"{bname}_{iteration}" in failed_benchmarks:
        return
    copy_dir()
    os.chdir(f"{results}/{name}/{bname}_{iteration}")
    config = get_config()
    run_ref = f"{gem5} {redirect} {se} {config} "\
              f"-c {bname} -o \"{options}\""
    print(run_ref)
    print(f"Finished with code {os.system(run_ref)}")
    os.chdir(f'{gem5_root}')
    move_result()
    cleanup()

run_benchmark()

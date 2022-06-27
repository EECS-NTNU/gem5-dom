from multiprocessing import Process
import os
import time
import shutil
import sys

work_root = os.getcwd()
gem5_root=f"{work_root}"
gem5=f"{gem5_root}/build/ARM/gem5.fast"
se=f"{gem5_root}/configs/example/fs.py"
graph_root=f"{gem5_root}/dom/arm-graphs"
stats="m5out/stats.txt"
results=f"{gem5_root}/results"

print(sys.argv)

index = int(sys.argv[1])
config_file = sys.argv[2]
name = sys.argv[3]

bname = ""
iteration = ""

with open(f"{graph_root}/commandlines") as commands:
    lines = commands.readlines()
    bname = lines[index].split(" ")[0][2:]
    options = lines[index].split(" ", 1)[1][:-1]

fs=f"--checkpoint-dir {bname}-cpt --disk-image " \
   f"{gem5_root}/../fs/ubuntu-18-arm.img " \
   f"--kernel {gem5_root}/../fs/vlinux-arm "\
   f"--script {gem5_root}/run_scripts/graphs/{bname}.rcS "\
   f"--bootloader={gem5_root}/../fs/boot-arm-2"
ckpt = "--fast-forward 9950000000 --at-instruction "\
       "--take-checkpoint=1000000000 --cpu-type=DerivO3CPU"
#ckpt = "-r 1 --cpu-type DerivO3CPU --restore-with-cpu DerivO3CPU"

options = ""

redirect=f"-r --stdout-file={bname}.simout"

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
    dst = f'{results}/{name}/{bname}'
    os.mkdir(dst)

def cleanup():
    tgt = f'{results}/{name}/{bname}'
    shutil.rmtree(tgt)

def save_cpt():
    src = f"{gem5_root}/full_cpts/{bname}/{bname}-cpt"
    dst = f"{gem5_root}/cpts/{bname}-cpt"
    shutil.copytree(src, dst)

def copy_cpt():
    if (os.path.isdir(f"{bname}-cpt")):
        shutil.rmtree(f"{bname}-cpt")
    src = f"{gem5_root}/cpts/{bname}-cpt"
    dst = f"{bname}-cpt"
    shutil.copytree(src, dst)
    src_2 = f"{dst}/cpt.None.10000000000"
    dst_2 = f"{dst}/cpt.1"
    shutil.copytree(src_2, dst_2)

def scrub_switch_cpu():
    with open(f"{bname}-cpt/cpt.1/m5.cpt", "r+") as cpt:
        data = cpt.read().replace("switch_cpus", "cpu")
        cpt.seek(0)
        cpt.write(data)
        cpt.truncate()

def setup_cpt():
    if (os.path.isdir(f"{bname}-cpt")):
        shutil.rmtree(f"{bname}-cpt")
    os.mkdir(f"{bname}-cpt")

def move_result():
    src = f'{results}/{name}/{bname}/m5out/stats.txt'
    dst = f'{results}/{name}/results/{bname}.txt'
    shutil.copy(src, dst)
    src = f'{results}/{name}/{bname}/m5out/{bname}.simout'
    dst = f'{results}/{name}/results/{bname}.simout'
    shutil.copy(src, dst)
    src = f'{results}/{name}/{bname}/m5out/system.terminal'
    dst = f'{results}/{name}/results/{bname}.stdout'
    shutil.copy(src, dst)
    src = f'{results}/{name}/{bname}/m5out/config.ini'
    dst = f'{results}/{name}/results/{bname}.config'
    shutil.copy(src, dst)
    src = f'{results}/{name}/{bname}/{bname}-cpt'
    dst = f'runs/cpts/graphs/{bname}-cpt'
    shutil.copytree(src, dst)

def run_benchmark():
    setup_dir()
    os.chdir(f"{results}/{name}/{bname}")
    config = get_config()
    #setup_cpt()
    #save_cpt()
    #copy_cpt()
    #scrub_switch_cpu()
    run_ref = f"{gem5} {redirect} {se} {fs} {config} {ckpt}"
    print(run_ref)
    print(f"Finished with code {os.system(run_ref)}")
    os.chdir(f'{gem5_root}')
    move_result()
    cleanup()

run_benchmark()

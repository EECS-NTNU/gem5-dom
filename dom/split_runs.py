from spec2006_commands import benchmarks
from multiprocessing import Process
import os
import time

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
spec_root=f"{gem5_root}/dom/x86-spec-all-ref"
results=f"{gem5_root}/results"

runCPU="--cpu-type=DerivO3CPU"
memory="--mem-size=8GB"
caches="--caches --l1d_size=32768 --l1i_size=32768 --l2_size=2097152 "\
"--l3_size=16MB --l1d_assoc=4 --l1i_assoc=4 "\
"--l2_assoc=8 --l3_assoc=16 --cacheline_size=64"

fast_forward="--fast-forward 1000000000"
runtime="--maxinsts=1000000000"

deldir = f"rm -r {gem5_root}/results"
print(os.system(deldir))

mkdir = f"mkdir {gem5_root}/results"
print(os.system(mkdir))

active_benchmarks = [False] * len(benchmarks)
running_processes = []

def execute_run(run, benchmark, num):
    os.chdir(f"{spec_root}/{benchmark.fullname}")
    b_name = benchmark.name
    print(f"Executing run {num + 1} of "\
        f"{benchmark.num_runs_ref()} for {b_name}")
    print(f"Run {num+1} for {b_name} finished "\
        f"with code {os.system(run)}")

def get_runs(benchmark):
    run_num = 0

    b_name = benchmark.name
    b_fullname = benchmark.fullname

    os.chdir(f"{spec_root}/{b_fullname}")
    runs = []
    for run in benchmark.get_runs_ref():
        run_file=f"-r --stdout-file={b_name}_{run_num}.out"
        stat_file=f"--stats-file={b_name}_{run_num}.txt"
        run_ref = f"{gem5} {run_file} {stat_file} {se} "\
        f"{fast_forward} {memory} {runtime} {caches} {runCPU} "\
        f"-c {b_name} -o \"{run}\""
        runs.append(run_ref)
        run_num = run_num + 1

    return runs

def move_result(benchmark):
    b_name = benchmark.name
    b_fullname = benchmark.fullname

    os.chdir(f"{spec_root}/{b_fullname}")

    for i in range(benchmark.num_runs_ref()):
        move_out = f"mv m5out/{b_name}_{i}.out {work_root}/results/"
        os.system(move_out)
        move_stat = f"mv m5out/{b_name}_{i}.txt {work_root}/results/"
        os.system(move_stat)

def all_running(nested):
    count = 0
    for x in nested:
        for y in x:
            if y.is_alive():
                count = count + 1
    return count

def get_running(processes):
    count = 0
    for x in processes:
        if x.is_alive():
            count = count + 1
    return count

def mover():
    print("Looking for results to move")
    print(active_benchmarks)
    print(running_processes)
    for x in range(len(active_benchmarks)):
        if (active_benchmarks[x]):
            print(f"Moving {benchmarks[x].fullname}")
            if get_running(running_processes[x]) == 0:
                move_result(benchmarks[x])
                active_benchmarks[x] = False


def run_managed(num):
    remaining_runs = []
    for benchmark in benchmarks:
        remaining_runs.append(get_runs(benchmark))
    for i in range(len(benchmarks)):
        bench_processes = []
        for j in range(len(remaining_runs[i])):
            while(get_running(bench_processes) +
                  all_running(running_processes)) >= num:
                print("Waiting to free processes")
                time.sleep(60)
            process = Process(target=execute_run,
                args=(remaining_runs[i][j], benchmarks[i], j,))
            process.start()
            bench_processes.append(process)
        running_processes.append(bench_processes)
        active_benchmarks[i] = True
        mover()
    while (all_running(running_processes) > 0):
        time.sleep(60)
        mover()
    mover()


run_managed(8)

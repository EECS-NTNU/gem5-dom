from spec2006_commands import benchmarks
from multiprocessing import Process
import os

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
stats="m5out/stats.txt"
results=f"{gem5_root}/results"

warmupCPU="--cpu-type=kvmCPU"
runCPU="--cpu-type=DerivO3CPU"
memory="--mem-size=32GB"
caches="--caches --l1d_size=64kB --l1i_size=16kB --l2_size=2MB "\
"--l3_size=16MB --l1d_assoc=2 --l1i_assoc=2 "\
"--l2_assoc=8 --l3_assoc=16 --cacheline_size=64"

fast_forward="--fast-forward 1000000000"
runtime="--maxinsts=3000000000"

mkdir = f"mkdir {gem5_root}/results"
print(os.system(mkdir))

def run_benchmark(benchmark):
    run_num = 0

    b_name = benchmark.name
    b_fullname = benchmark.fullname

    print(f"Now processing {b_fullname}")

    os.chdir(f"{spec_root}/{b_fullname}")
    print(f"Now running {b_fullname}")
    for run in benchmark.get_runs_ref()[0:1]:
        run_file=f"-r --stdout-file={b_name}_{run_num}.out"
        print(f"Executing run {run_num + 1} of "\
        f"{benchmark.num_runs_ref()} for {b_name}")
        run_ref = f"{gem5} {run_file} {se} {fast_forward} {memory}"\
        f"{runtime} {caches} {runCPU} -c {b_name} -o \"{run}\""
        print(run_ref)
        print(f"Run {run_num+1} for {b_name} finished "\
        f"with code {os.system(run_ref)}")

        move_data = f"mv {stats} {results}/{b_name}_{run_num}.txt"
        os.system(move_data)
        run_num = run_num + 1

    move_result(benchmark)

def move_result(benchmark):
    b_name = benchmark.name
    b_fullname = benchmark.fullname

    os.chdir(f"{spec_root}/{b_fullname}")

    for i in range(1):#benchmark.num_runs_ref()[0:1]):
        move = f"mv m5out/{b_name}_{i}.out {work_root}/results/"
        os.system(move)

processes = []
run_benchmark(benchmarks[0])
exit(1)
for benchmark in benchmarks:
    p = Process(target=run_benchmark, args=(benchmark,))
    p.start()

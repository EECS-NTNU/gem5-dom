import os

def compile_data():
    now = dt.datetime.now()
    print(f"Compiling data for {b_name} at {now}")
    # Compile Data according to weights
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

    with open(f"{sims}_compiled_stats.txt", 'w') as w,
    open(f"{sims}.weights.lpt0.9") as weights:
        weight = weights.readline().split()[0]
        w.write(f"Relative Weight: {weight}\n")
        with open(f"{sims}_stat{x}.txt") as f:
            stats_data = {}
            f.readline()
            f.readline()
            line = f.readline()
            while(line.strip()):
                print(line)
                line = line.split()
                if line[0] in stats:
                    stats_data[line[0]] = line[1]
                line = f.readline()
            print(stats_data, file=w)
            w.write("\n\n")

b_name="mcf"
b_fullname="429.mcf"
spec_root="/home/amundbk/Documents/Research/Speckle/x86-spec-ref"
b_com= f"{spec_root}/{b_fullname}/{b_name}"
b_opt= f"\"{spec_root}/{b_fullname}/inp.in\""
com =  f"-c {b_com} -o {b_opt}"

gem5="./build/X86/gem5.opt"
se="./configs/example/se.py"
out="m5out/"
data=f"results/{b_name}/"
results=f"{data}{b_name}"

warmupCPU="--cpu-type=kvmCPU"
runCPU="--cpu-type=DerivO3CPU"
caches="--caches --l1d_size=64kB --l1i_size=16kB",
"--l2_size=2MB --l3_size=16MB --l1d_assoc=2",
"--l1i_assoc=2 --l2_assoc=8 --l3_assoc=16 --cacheline_size=64"

fast_forward="--fast-forward 300000000"
switch="--maxinsts=100000000"
output=f"--output=/{results}/stats.txt"

mkdir = f"mkdir {data}"
print(os.system(mkdir))

base_data=f"{gem5} {se} {fast_forward} {switch} {caches} {runCPU} {com}"
print(os.system(base_data))

stat_mov = f"mv {out}stats.txt {results}/{b_name}_1.txt"
print(os.system(stat_mov))

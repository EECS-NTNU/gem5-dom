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

    with open(f"{sims}_compiled_stats.txt", 'w') as w, \
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

b_name="Xalan"
b_fullname="483.xalancbmk"
spec_root="/home/amundbk/Documents/Research/boot-tests/gem5/dom/x86-spec-ref"
b_com= f"{b_name}"
b_opt= f"\"-v t5.xml xalanc.xsl\""
com =  f"-c {b_com} -o {b_opt}"

gem5_root="/home/amundbk/Documents/Research/boot-tests/gem5"
gem5=f"{gem5_root}/build/X86/gem5.opt"
se=f"{gem5_root}/configs/example/se.py"
out="m5out/"
data=f"results/{b_name}/"
results=f"{data}{b_name}"

warmupCPU="--cpu-type=kvmCPU"
runCPU="--cpu-type=DerivO3CPU"
caches="--caches --l1d_size=64kB --l1i_size=16kB --l2_size=2MB"\
    "--l3_size=16MB --l1d_assoc=2 --l1i_assoc=2 --l2_assoc=8"\
    "--l3_assoc=16 --cacheline_size=64"

fast_forward="--fast-forward 30000000"
switch="--maxinsts=10000000"
output=f"--output=/{results}/stats.txt"

mkdir = f"mkdir {data}"
print(os.system(mkdir))

move_to_b=f"{spec_root}/{b_fullname}"
os.chdir(move_to_b)

print(os.system("ls"))

base_data=f"{gem5} {se} {fast_forward} {switch} {caches} {runCPU} {com}"
print(base_data)
print(os.system(base_data))

stat_mov = f"mv {out}stats.txt {results}/{b_name}_1.txt"
print(os.system(stat_mov))

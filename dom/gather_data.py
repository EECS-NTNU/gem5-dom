import os
import datetime as dt

def run():
    profile()
    create_simpoints()
    create_checkpoints()
    run_with_checkpoints()
    compile_data()

def profile():
    now = dt.datetime.now()
    print(f"Starting operation at {now}.")
    print(f"Profiling {b_name}")
    # Profile Benchmark
    create_profile = f"{gem5} {se} --simpoint-profile"\
        f" --simpoint-interval 10000000 {profileCPU} {com}"
    print(os.system(create_profile))


def create_simpoints():
    now = dt.datetime.now()
    print(f"Creating simpoints at {now}")
    # Create Simpoints with 95% coverage or better
    create_sim = f"{simpoints} -loadFVFile {out}simpoint.bb.gz"\
        f"-maxK 10 -coveragePct 0.90 -saveSimpoints {sims}.sim"\
        f"-saveSimpointWeights {sims}.weights -inputVectorsGzipped"
    print(os.system(create_sim))

def create_checkpoints():
    now = dt.datetime.now()
    print(f"Creating checkpoints at {now}")
    # Create Checkpoints
    create_check = f"{gem5} {se}"\
        f"--take-simpoint-checkpoint={sims}.sim.lpt0.9,"\
            f"{sims}.weights.lpt0.9,10000000,10000000"\
            f" {profileCPU} {com}"
    print(os.system(create_check))


    #Move Checkpoints
    global num_sims
    num_sims = 0
    with open(f"{sims}.sim.lpt0.9") as f:
        num_sims = sum(1 for _ in f)

    check_mov= f"mv {out}cpt.simpoint_* {data}"
    print(os.system(check_mov))

def run_with_checkpoints():
    # Run with checkpoints, copy data
    for x in range(num_sims):
        now = dt.datetime.now()
        print(f"Getting data for checkpoint {x} at {now}")
        # Do the run
        run_check = f"{gem5} {se} --restore-simpoint-checkpoint"\
            " -r {x+1} --checkpoint-dir={data} {runCPU} {caches} {com}"
        print(os.system(run_check))
        stat_mov = f"mv {out}stats.txt {sims}_stat{x}.txt"
        print(os.system(stat_mov))

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

    for x in range(num_sims):
        with open(f"{sims}_compiled_stats.txt", 'w') as w,\
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
spec_root="/home/amundbk/Documents/Research/Speckle/x86-spec-ref"
b_com= f"{spec_root}/{b_fullname}/{b_name}"
b_opt= f"\"-v {spec_root}/{b_fullname}/t5.xml"\
    f" {spec_root}/{b_fullname}/xalanc.xsl\""
com =  f"-c {b_com} -o {b_opt}"

gem5="./build/X86/gem5.opt"
se="./configs/example/se.py"
out="m5out/"
data=f"simpoints/{b_name}/"
sims=f"{data}{b_name}"

profileCPU="--cpu-type=NonCachingSimpleCPU"
runCPU="--cpu-type=DerivO3CPU"
caches="--caches --l1d_size=64kB --l1i_size=16kB "\
    "--l2_size=2MB --l3_size=16MB --l1d_assoc=2 "\
    "--l1i_assoc=2 --l2_assoc=8 --l3_assoc=16 "\
    "--cacheline_size=64"

simpoints="../SimPoint/bin/simpoint"

mkdir = f"mkdir {data}"
print(os.system(mkdir))
run()

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

runCPU="--cpu-type=DerivO3CPU"
memory="--mem-size=8GB"
prefetcher="--l1d-hwp-type=StridePrefetcher --l2-hwp-type=StridePrefetcher"
caches="--caches --l1d_size=32768 --l1i_size=32768 --l2_size=2097152 "\
"--l2cache --l3_size=16MB --l1d_assoc=4 --l1i_assoc=4 "\
"--l3cache --l2_assoc=8 --l3_assoc=16 --cacheline_size=64"

mp_args="--mp_mode --ap_mode --confidence_saturation=10 "\
"--confidence_threshold=8 --confidence_up_step=1 "\
"--confidence_down_step=3"

pred_args="--pred_delay=3 --prune_ready --pred_shadows_only"


fast_forward="--fast-forward 3000000000"
runtime="--maxinsts=1000000000"

print(sys.argv)

cmd = sys.argv[1]
options = sys.argv[2]

redirect=f"-r --stdout-file=test.simout"

def run_benchmark():
    run_ref = f"{gem5} {redirect} {se} {fast_forward} {memory} "\
        f"{runtime} {mp_args} {prefetcher} {caches} {pred_args} "\
        f"{runCPU} -c {cmd} -o \"{options}\""

    print(f"Finished with code {os.system(run_ref)}")

run_benchmark()

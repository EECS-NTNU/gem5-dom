
class Benchmark:
    def __init__(self, name, fullname):
        self.name = name
        self.fullname = fullname
        self.refs = []

    def add_ref(self, command):
        self.refs.append(command)

    def num_runs_ref(self):
        return len(self.refs)

    def get_runs_ref(self):
        return self.refs

perlbench = Benchmark("perlbench", "400.perlbench")
perlbench.add_ref("-I./lib checkspam.pl 2500 5 25 11 150 1 1 1 1")
perlbench.add_ref("-I./lib diffmail.pl 4 800 10 17 19 300")
perlbench.add_ref("-I./lib splitmail.pl 1600 12 26 16 4500")

bzip2 = Benchmark("bzip2", "401.bzip2")
bzip2.add_ref("input.source 200")
bzip2.add_ref("chicken.jpg 30")
bzip2.add_ref("liberty.jpg 30")
bzip2.add_ref("input.program 280")
bzip2.add_ref("text.html 280")
bzip2.add_ref("input.combined 200")

gcc = Benchmark("gcc", "403.gcc")
gcc.add_ref("166.in -o 166.s")
gcc.add_ref("200.in -o 200.s")
gcc.add_ref("c-typeck.in -o c-typeck.s")
gcc.add_ref("cp-decl.in -o cp-decl.s")
gcc.add_ref("expr.in -o expr.s")
gcc.add_ref("expr2.in -o expr2.s")
gcc.add_ref("g23.in -o g23.s")
gcc.add_ref("s04.in -o s04.s")
gcc.add_ref("scilab.in -o scilab.s")

mcf = Benchmark("mcf", "429.mcf")
mcf.add_ref("inp.in")

gobmk = Benchmark("gobmk", "445.gobmk")
gobmk.add_ref("--quiet --mode gtp < 13x13.tst")
gobmk.add_ref("--quiet --mode gtp < nngs.tst")
gobmk.add_ref("--quiet --mode gtp < score2.tst")
gobmk.add_ref("--quiet --mode gtp < trevorc.tst")
gobmk.add_ref("--quiet --mode gtp < trevord.tst")

hmmer = Benchmark("hmmer", "456.hmmer")
hmmer.add_ref("nph3.hmm swiss41")
hmmer.add_ref("--fixed 0 --mean 500 --num 500000 --sd 350 --seed 0 retro.hmm")

sjeng = Benchmark("sjeng", "458.sjeng")
sjeng.add_ref("ref.txt")

libquantum = Benchmark("libquantum", "462.libquantum")
libquantum.add_ref("1397 8")

h264ref = Benchmark("h264ref", "464.h264ref")
h264ref.add_ref("-d foreman_ref_encoder_baseline.cfg")
h264ref.add_ref("-d foreman_ref_encoder_main.cfg")
h264ref.add_ref("-d sss_encoder_main.cfg")

omnetpp = Benchmark("omnetpp", "471.omnetpp")
omnetpp.add_ref("omnetpp.ini")

astar = Benchmark("astar", "473.astar")
astar.add_ref("BigLakes2048.cfg")
astar.add_ref("rivers.cfg")

xalan = Benchmark("Xalan", "483.xalancbmk")
xalan.add_ref("-v t5.xml xalanc.xsl")

bwaves = Benchmark("bwaves", "410.bwaves")
bwaves.add_ref("< bwaves.in")

gamess = Benchmark("gamess", "416.gamess")
gamess.add_ref("< cytosine.2.config")
gamess.add_ref("< h2ocu2+.gradient.config")
gamess.add_ref("< triazolium.config")

milc = Benchmark("milc", "433.milc")
milc.add_ref("< su3imp.in")

zeusmp = Benchmark("zeusmp", "434.zeusmp")
zeusmp.add_ref("< zmp_inp")

gromacs = Benchmark("gromacs", "435.gromacs")
gromacs.add_ref("-silent -deffnm gromacs -nice 0")

cactusADM = Benchmark("cactusADM", "436.cactusADM")
cactusADM.add_ref("benchADM.par")

leslie3d = Benchmark("leslie3d", "437.leslie3d")
leslie3d.add_ref("< leslie3d.in")

namd = Benchmark("namd", "444.namd")
namd.add_ref("--input namd.input --iterations 38 --output namd.out")

dealII = Benchmark("dealII", "447.dealII")
dealII.add_ref("23")

soplex = Benchmark("soplex", "450.soplex")
soplex.add_ref("-s1 -e -m45000 pds-50.mps")
soplex.add_ref("-m3500 ref.mps")

povray = Benchmark("povray", "453.povray")
povray.add_ref("SPEC-benchmark-ref.ini")

calculix = Benchmark("calculix", "454.calculix")
calculix.add_ref("-i hyperviscoplastic")

GemsFDTD = Benchmark("GemsFDTD", "459.GemsFDTD")
GemsFDTD.add_ref("< test.in")

tonto = Benchmark("tonto", "465.tonto")
tonto.add_ref("< stdin")

lbm = Benchmark("lbm", "470.lbm")
lbm.add_ref("3000 reference.dat 0 0 100_100_130_ldc.of")

wrf = Benchmark("wrf", "481.wrf")
wrf.add_ref("< namelist.input")

sphinx3 = Benchmark("sphinx3", "482.sphinx3")
sphinx3.add_ref("ctlfile . args.an4")

int_benchmarks = [
    perlbench,
    bzip2,
    gcc,
    mcf,
    gobmk,
    hmmer,
    sjeng,
    libquantum,
    h264ref,
    omnetpp,
    astar,
    xalan
]

fp_benchmarks = [
    bwaves,
    gamess,
    milc,
    zeusmp,
    gromacs,
    cactusADM,
    leslie3d,
    namd,
    dealII,
    soplex,
    povray,
    calculix,
    GemsFDTD,
    tonto,
    lbm,
    wrf,
    sphinx3
]

benchmarks = []
benchmarks.extend(int_benchmarks)
benchmarks.extend(fp_benchmarks)

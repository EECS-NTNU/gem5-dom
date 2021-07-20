
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

benchmarks = []
benchmarks.append(perlbench)
benchmarks.append(gcc)
benchmarks.append(mcf)
benchmarks.append(libquantum)
benchmarks.append(h264ref)
benchmarks.append(omnetpp)
benchmarks.append(astar)
benchmarks.append(xalan)

import optparse
import sys
import os
from box import Box

import m5
from m5.defines import buildEnv
from m5.objects import *
from m5.params import NULL
from m5.util import addToPath, fatal, warn

addToPath('../')
addToPath('../configs')

from common import Options
from common import Simulation
from common import CacheConfig
from common import CpuConfig
from common import ObjectList
from common import MemConfig
from common.FileSystemConfig import config_filesystem
from common.Caches import *
from common.cpu2000 import *

optionsDict = {
 'num_cpus': 1,
 'sys_voltage': '1.0V',
 'sys_clock': '1GHz',
 'mem_type': 'DDR3_1600_8x8',
 'mem_channels': 1,
 'mem_ranks': None,
 'mem_size': '512MB',
 'enable_dram_powerdown': None,
 'mem_channels_intlv': 0,
 'memchecker': None,
 'external_memory_system': None,
 'tlm_memory': None,
 'caches': True,
 'l2cache': True,
 'num_dirs': 1,
 'num_l2caches': 1,
 'num_l3caches': 1,
 'l1d_size': '64kB',
 'l1i_size': '16kB',
 'l2_size': '2MB',
 'l3_size': '16MB',
 'l1d_assoc': 2,
 'l1i_assoc': 2,
 'l2_assoc': 8,
 'l3_assoc': 16,
 'cacheline_size': 64,
 'ruby': None,
 'abs_max_tick': 18446744073709551615,
 'rel_max_tick': None,
 'maxtime': None,
 'param': [],
 'cpu_type': 'DerivO3CPU',
 'bp_type': None,
 'indirect_bp_type': None,
 'l1i_hwp_type': None,
 'l1d_hwp_type': None,
 'l2_hwp_type': None,
 'checker': None,
 'cpu_clock': '2GHz',
 'smt': False,
 'elastic_trace_en': None,
 'inst_trace_file': '',
 'data_trace_file': '',
 'lpae': None,
 'virtualisation': None,
 'dist': None,
 'dist_sync_on_pseudo_op': None,
 'is_switch': None,
 'dist_rank': 0,
 'dist_size': 0,
 'dist_server_name': '127.0.0.1',
 'dist_server_port': 2200,
 'dist_sync_repeat': '0us',
 'dist_sync_start': '5200000000000t',
 'ethernet_linkspeed': '10Gbps',
 'ethernet_linkdelay': '10us',
 'maxinsts': None,
 'work_item_id': None,
 'num_work_ids': None,
 'work_begin_cpu_id_exit': None,
 'work_end_exit_count': None,
 'work_begin_exit_count': None,
 'init_param': 0,
 'initialize_only': False,
 'simpoint_profile': None,
 'simpoint_interval': 10000000,
 'take_simpoint_checkpoints': None,
 'restore_simpoint_checkpoint': None,
 'take_checkpoints': None,
 'max_checkpoints': 5,
 'checkpoint_dir': None,
 'checkpoint_restore': None,
 'checkpoint_at_end': None,
 'work_begin_checkpoint_count': None,
 'work_end_checkpoint_count': None,
 'work_cpus_checkpoint_count': None,
 'restore_with_cpu': 'AtomicSimpleCPU',
 'repeat_switch': None,
 'standard_switch': None,
 'prog_interval': None,
 'warmup_insts': None,
 'bench': None,
 'fast_forward': None,
 'simpoint': False,
 'at_instruction': False,
 'spec_input': 'ref',
 'arm_iset': 'arm',
 'stats_root': [],
 'cmd': 'dry2o',
 'options': '',
 'env': '',
 'input': '',
 'output': '',
 'errout': '',
 'chroot': None,
 'interp_dir': None,
 'redirects': [],
 'wait_gdb': False
}

options = Box(optionsDict, frozen_box=True)

numThreads = 1

(CPUClass, test_mem_mode, FutureClass) = Simulation.setCPUClass(options)
CPUClass.numThreads = numThreads


isa = str(m5.defines.buildEnv['TARGET_ISA']).lower()

process = Process(pid = 100)
#process.executable =
# "./dom/429.mcf/run/run_base_test_ia64-gcc42.0000/mcf_base.ia64-gcc42"
#process.cmd=
# ["./dom/429.mcf/run/run_base_test_ia64-gcc42.0000/mcf_base.ia64-gcc42",
#"./dom/429.mcf/run/run_base_test_ia64-gcc42.0000/inp.in"]

process.executable = "./tests/test-progs/hello/bin/x86/linux/hello"
process.cmd = "./tests/test-progs/hello/bin/x86/linux/hello"

process.cwd = os.getcwd()

system = System(cpu = [CPUClass(cpu_id=i) for i in range(1)],
                mem_mode = test_mem_mode,
                mem_ranges = [AddrRange(options.mem_size)],
                cache_line_size = options.cacheline_size,
                workload = SEWorkload.init_compatible(process.executable))


# Create a top-level voltage domain
system.voltage_domain = VoltageDomain(voltage = options.sys_voltage)

# Create a source clock for the system and set the clock period
system.clk_domain = SrcClockDomain(clock =  options.sys_clock,
                                   voltage_domain = system.voltage_domain)

# Create a CPU voltage domain
system.cpu_voltage_domain = VoltageDomain()

# Create a separate clock domain for the CPUs
system.cpu_clk_domain = SrcClockDomain(clock = options.cpu_clock,
                                       voltage_domain =
                                       system.cpu_voltage_domain)


system.cpu[0].workload = process
system.cpu[0].createThreads()

MemClass = Simulation.setMemClass(options)
system.membus = SystemXBar()
system.system_port = system.membus.slave
CacheConfig.config_cache(options, system)
MemConfig.config_mem(options, system)
config_filesystem(system, options)

root = Root(full_system = False, system = system)
Simulation.run(options, root, system, FutureClass)

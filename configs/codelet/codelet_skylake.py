# -*- coding: utf-8 -*-
# Copyright (c) 2020 The Regents of the University of California
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met: redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer;
# redistributions in binary form must reproduce the above copyright
# notice, this list of conditions and the following disclaimer in the
# documentation and/or other materials provided with the distribution;
# neither the name of the copyright holders nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# Authors: Jason Lowe-Power, Trivikram Reddy, Dawson Fox


""" This file creates a codelet system with a CodeletInterface
for a single x86 Skylake CPU and Scheduling Unit (SU). The system includes 
a full cache subsystem according to the skylake configuration of gem5
The SCM program path is hardcoded and this system is intended to be ran with 
the workload set as the CU runtime with user defined Codelets compiled into it.
This config is currently working with CPU types Tuned and Verbatim. The Unconstrained
skylake CPU will throw an error because the fetchWidth is larger than the max limit.

This config file assumes that the x86 ISA was built.
"""


import m5
from m5.objects import *
import argparse
#from system.se  import MySystem
from system.se  import MyCodeletSystem
from system.core import *

#valid_configs = [VerbatimCPU, TunedCPU, UnConstrainedCPU]
valid_configs = [VerbatimCPU, TunedCPU, UnconstrainedCPU]
valid_configs = {cls.__name__[:-3]:cls for cls in valid_configs}

parser = argparse.ArgumentParser()
parser.add_argument('config', choices = valid_configs.keys())
parser.add_argument('num_cores', type = int, help = "Number of cores to instantiate")
parser.add_argument('binary', type = str, help = "Path to binary to run")
args = parser.parse_args()

#class TestSystem(MySystem):
class TestSystem(MyCodeletSystem):
    _CPUModel = valid_configs[args.config]
    def __init__(self, scmProgPath, numCores):
        super(TestSystem, self).__init__(scmProgPath, numCores)


# For now, hardcode scm program path
scm_file_name = "/home/dfox/gem5_cod/tests/test-progs/codelet/src/test_prog.scm"
system = TestSystem(scm_file_name, args.num_cores)
system.setTestBinary(args.binary, args.num_cores)
root = Root(full_system = False, system = system)
m5.instantiate()

# With multiple CodeletInterfaces, we need to make sure in the CU runtime the addresses are checked properly
for i in range(args.num_cores):
    system.cpu[i].workload[0].map(Addr(0x90000000),
            Addr(0x90000000),
            #0xf0,
            0x44 * args.num_cores + 0x80, #Extending to cover space of multiple CodeletInterfaces and the SU 
            False)
    
    system.cpu[i].workload[0].map(Addr(0x90001000),
            Addr(0x90001000),
            12288000); # Attempting to map a fixed register space 
            # Size comes from the SCM register config

print("Beginning simulation!")
exit_event = m5.simulate()
print("Exiting @ tick %i because %s" % (m5.curTick(), exit_event.getCause()))

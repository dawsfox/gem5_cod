from m5.params import *
from m5.proxy import *
from m5.objects.ClockedObject import ClockedObject

class CodeletInterface(ClockedObject):
    type = 'CodeletInterface'
    cxx_header = "codelet/codelet_interface.hh"
    cxx_class = "gem5::CodeletInterface"

    #Needs same ports as a cache since it will pass through normal memory requests -- i port and d port and pop/read codelets?
    cpu_side_ports = VectorResponsePort("CPU side port, services mem requests, pop Codelets (memory mapped)")
    #Mem side for forwarding normal memory requests to membus
    mem_side_port = RequestPort("Memory side port, creates requests")
    #Codelet side request ports for retiring codelets and signaling dependencies
    cod_side_req_port = RequestPort("Request ports for signaling dependencies and retiring Codelets")
    #Codelet side response port for receiving Codelets from SU
    cod_side_resp_port = ResponsePort("Response port for pushing Codelets from SU")

    queue_range = Param.AddrRange(AddrRange(start = Addr(0x90000000), end = Addr(0x90000000) + 0x40), "Address range used by local Codelet queue")
    cu_id = Param.Unsigned(0, "ID number for the CU the codelet interface represents")
    su_ret_addr = Param.Addr(0x90000040, "Addres used for codelet retirement packets forwarded to SU")
    queue_latency = Param.Cycles(1, "Cycles delayed to process Codelet queue actions")
    gen_latency = Param.Cycles(0, "Cycles delayed for passing through requests")
    size = Param.MemorySize("16kB", "Codelet queue size")
    system = Param.System(Parent.any, "The system the interface is part of")
    
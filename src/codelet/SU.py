from m5.params import *
from m5.proxy import *
from m5.objects.ClockedObject import ClockedObject

class SU(ClockedObject):
    type = 'SU'
    cxx_header = "codelet/su.hh"
    cxx_class = "gem5::SU"

    #Codelet side response port for receiving Codelets from SU
    cod_side_resp_ports = VectorResponsePort("Response ports for handling decDep signaling and Codelet retiring")
    cod_side_req_port =  RequestPort("Request port for pushing Codelets to CUs")


    su_sig_range = Param.AddrRange("Address range used by SU for dependency signalling")
    su_ret_range = Param.AddrRange("Address range used by SU for codelet retirement")

    sig_latency = Param.Cycles(1, "Cycles delayed for signaling syncSlots")
    size = Param.MemorySize("16kB", "SyncSlot storage size")
    system = Param.System(Parent.any, "The system the interface is part of")
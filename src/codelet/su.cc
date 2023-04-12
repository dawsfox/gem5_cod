
#include "codelet/su.hh"

#include "debug/SU.hh"
#include "sim/system.hh"

namespace gem5
{

#define SYNCSLOT_SIZE 32 //just a placeholder value...

SU::SU(const SUParams &params) :
    ClockedObject(params),
    sigLatency(params.sig_latency),
    capacity(params.size / SYNCSLOT_SIZE),
    suRange(params.su_range),
    codReqPort(params.name + ".cod_side_req_port", this),
    blocked(false), originalPacket(nullptr), waitingPortId(-1), stats(this)
{
    for (int i = 0; i < params.port_cod_side_resp_ports_connection_count; i++) {
        // again unsure of the param name ...
        codRespPorts.emplace_back(name() + csprintf(".cod_side_resp_ports[%d]", i), i, this);
    }
}

Port &
SU::getPort(const std::string &if_name, PortID idx)
{
    // This is the name from the Python SimObject declaration
    if (if_name == "cod_side_req_port") {
        panic_if(idx != InvalidPortID,
                 "Cod side req of codelet interface not a vector port");
        return(codReqPort);
    } else if (if_name == "cod_side_resp_ports" && idx < codRespPorts.size()) {
        // We should have already created all of the ports in the constructor
        return(codRespPorts[idx]);
    } else {
        // pass it along to our super class
        return(ClockedObject::getPort(if_name, idx));
    }
}

void
SU::CodSideReqPort::recvRangeChange()
{
    DPRINTF(SU, "SU receiving range change\n");
    // indicates range change coming from CodeletInterface
    owner->sendRangeChange();
}

bool
SU::CodSideRespPort::recvTimingReq(PacketPtr pkt)
{
    DPRINTF(SU, "SU receiving timing request\n");
    return(false);
}

void
SU::sendRangeChange() const
{
    DPRINTF(SU, "SU sending range change\n");
    // really would change the range of the SU local space...
    // does this conflict with the parameter assignment?
    // for now leave blank
}

AddrRangeList
SU::CodSideRespPort::getAddrRanges() const
{ 
    DPRINTF(SU, "getting new addr ranges for cod-side resp. ports\n");
    AddrRangeList ranges;
    ranges.push_back(owner->suRange);
    return(ranges);
}

SU::SUStats::SUStats(statistics::Group *parent)
      : statistics::Group(parent),
      ADD_STAT(cods, statistics::units::Count::get(), "Number of codelets scheduled"),
      ADD_STAT(sigs, statistics::units::Count::get(), "Number of dependencies signaled"),
      ADD_STAT(codLatency, statistics::units::Tick::get(),
               "Ticks for Codelet latency from scheduling to retirement")
{
    codLatency.init(16); // number of buckets, can change later
}

} // namespace gem5
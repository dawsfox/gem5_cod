
#include "codelet/codelet_interface.hh"
#include "codelet/codelet.hh"
//#include "base/compiler.hh"
//#include "base/random.hh"
#include "debug/CodeletInterface.hh"
#include "debug/CodeletInterfaceQueue.hh"
#include "debug/CodeletInterfaceCod.hh"
#include "sim/system.hh"
#include "base/loader/symtab.hh"
#include "sim/process.hh"

namespace gem5
{

// let's hardcode some function names here;
//_Z12helloCodFirev (from nm)
//std::string test_fire = "helloCodFire";
std::string test_fire = "_Z12helloCodFirev";

void pushCod(System *system, std::queue<codelet_t> *codQueue)
{
    ThreadContext *tmp_context = system->threads[0];
    loader::ObjectFile *tmp_obj = tmp_context->getProcessPtr()->objFile;
    loader::SymbolTable tmp_sym = tmp_obj->symtab();
    auto sym_it = tmp_sym.find(test_fire);
    if (sym_it != tmp_sym.end()) {
        // if sym found            
        DPRINTF(CodeletInterfaceQueue, "symbol found: %lx\n", sym_it->address);
        codelet_t testCod = {(fire_t)sym_it->address, (unsigned) 1};
        for (int i=0; i<30; i++) {
            codQueue->push(testCod);
        }
    } else {
        DPRINTF(CodeletInterfaceQueue, "symbol not found :(\n");
        DPRINTF(CodeletInterfaceQueue, "searched for %s : not found", test_fire);
    }

}

#define CODELET_SIZE 32 //just a placeholder value...
CodeletInterface::CodeletInterface(const CodeletInterfaceParams &params) :
    ClockedObject(params),
    queueLatency(params.queue_latency),
    genLatency(params.gen_latency),
    capacity(params.size / CODELET_SIZE),
    queueRange(params.queue_range),
    memPort(params.name + ".mem_side_port", this),
    codRespPort(params.name + ".cod_side_resp_port", this),
    blocked(false), originalPacket(nullptr), waitingPortId(-1), stats(this)
{
    // Since the CPU side ports are a vector of ports, create an instance of
    // the CPUSidePort for each connection. This member of params is
    // automatically created depending on the name of the vector port and
    // holds the number of connections to this port name
    for (int i = 0; i < params.port_cpu_side_ports_connection_count; ++i) {
        cpuPorts.emplace_back(name() + csprintf(".cpu_side_ports[%d]", i), i, this);
    }
    for (int i = 0; i < params.port_cod_side_req_ports_connection_count; i++) {
        // cod req ports here. and lets hope i got the name of the param right??
        codReqPorts.emplace_back(name() + csprintf(".cod_side_req_ports[%d]", i), i, this);
    }
    //for (int i = 0; i<5; i++) {
        // dumb workaround because of how lambdas work
        /*
        std::queue<codelet_t> * tmpQPtr = &codQueue;
            DPRINTF(CodeletInterfaceQueue, "symbol not found :(\n");
            schedule(new EventFunctionWrapper([this, params, tmpQPtr]{ pushCod(params.system, tmpQPtr); },
                                      name() + ".pushEvent", true),
                    clockEdge(queueLatency));
        */
    //} //for
}

Port &
CodeletInterface::getPort(const std::string &if_name, PortID idx)
{
    // This is the name from the Python SimObject declaration
    if (if_name == "mem_side_port") {
        panic_if(idx != InvalidPortID,
                 "Mem side of codelet interface not a vector port");
        return(memPort);
    } else if (if_name == "cpu_side_ports" && idx < cpuPorts.size()) {
        // We should have already created all of the ports in the constructor
        return(cpuPorts[idx]);
    } else if (if_name == "cod_side_req_ports" && idx < codReqPorts.size()) {
        return(codReqPorts[idx]);
    } else if (if_name == "cod_side_resp_port") {
        panic_if(idx != InvalidPortID,
                 "Cod side resp. of codelet interface not a vector port");
        return(codRespPort);
    } else {
        // pass it along to our super class
        return(ClockedObject::getPort(if_name, idx));
    }
}

// -------------------------------------------------------------------------------------------

void
CodeletInterface::CPUSidePort::sendPacket(PacketPtr pkt)
{
    panic_if(blockedPacket != nullptr, "Should never try to send if blocked!");

    // If we can't send the packet across the port, store it for later.
    DPRINTF(CodeletInterface, "Sending %s to CPU\n", pkt->print());
    if (!sendTimingResp(pkt)) {
        DPRINTF(CodeletInterface, "failed!\n");
        blockedPacket = pkt;
    }
}

void
CodeletInterface::CPUSidePort::trySendRetry()
{
    if (needRetry && blockedPacket == nullptr) {
        // Only send a retry if the port is now completely free
        needRetry = false;
        DPRINTF(CodeletInterface, "Sending retry req.\n");
        sendRetryReq();
    }
}

void
CodeletInterface::CPUSidePort::recvFunctional(PacketPtr pkt)
{
    // Just forward to the interface
    // handleFunctional should decide whether to forward or do a queue operation
    return owner->handleFunctional(pkt);
}


void
CodeletInterface::handleFunctional(PacketPtr pkt)
{
    // Here we check address to see if a queue interaction is needed
    Addr reqAddr = pkt->getAddr();
    if (queueRange.contains(reqAddr)) {
        if(accessFunctional(pkt)) {
            sendResponse(pkt);
            return;
        }
        else {
            // big question: what to do if the codelet pop fails? introduce a NullCodelet??
            // ignore for now....
            sendResponse(pkt);
        }
    }
    // also will have to add routing to CU / codelet ports here later.....
    else {
        memPort.sendFunctional(pkt);
    }
}

bool
CodeletInterface::accessFunctional(PacketPtr pkt)
{
    // this should be moved somewhere else; accessFunctional will probably be called
    // when the SU pushes to the queue as well. it's fine for now...
    //panic_if(!pkt->isRead(), "CPU should not do anything to queue space but read it");
    // pop codelet and return
    DPRINTF(CodeletInterfaceQueue, "received packet with addr %x and is write: %d\n", pkt->getAddr(), pkt->isWrite());
    if (!codQueue.empty() && pkt->isRead()) {
        codelet_t toPop = codQueue.front(); //get next Codelet up
        codQueue.pop(); // remove it from the queue
        // how to put it in a packet?
        pkt->makeResponse();
        auto data = pkt->getPtr<uint8_t>();
        std::memcpy(data, &(toPop.fire), sizeof(fire_t));
        //pkt->setSize(sizeof(codelet_t));
        //pkt->setSize(sizeof(fire_t));
        // no byte swapping, set new data
        DPRINTF(CodeletInterfaceQueue, "popping Codelet from queue to send\n");
        return(true);
    }
    else if (pkt->isWrite()) {
        auto pkt_data = pkt->getPtr<uint8_t>();
        codelet_t toPush;
        std::memcpy(&toPush, pkt_data, sizeof(codelet_t));
        codQueue.push(toPush);
        pkt->makeResponse(); // with no data modification, because it was a write
        DPRINTF(CodeletInterfaceQueue, "pushing Codelet from packet to queue\n");
        return(true);
    }
    else {
        return(false);
    }
}

void
CodeletInterface::accessTiming(PacketPtr pkt)
{
    // first do the access logic and check if it worked
    if (accessFunctional(pkt)) {
        // managed to actually pop a codelet and modify packet
        sendResponse(pkt); // send back to CPU 
    } else {
        // send response without changing; runtime should catch size isn't large enough to be a codelet
        pkt->makeResponse();
        auto data = pkt->getPtr<uint8_t>();
        auto size = pkt->getSize();
        std::fill(data, data + size, 0); //send nullptr basically
        sendResponse(pkt);
    }
}


bool
CodeletInterface::CPUSidePort::recvTimingReq(PacketPtr pkt)
{
    DPRINTF(CodeletInterface, "Got request %s\n", pkt->print());

    if (blockedPacket || needRetry) {
        // The interface may not be able to send a reply if this is blocked
        DPRINTF(CodeletInterface, "Request blocked\n");
        needRetry = true;
        return false;
    }
    // Just forward to the interface.
    if (!owner->handleRequest(pkt, id)) {
        DPRINTF(CodeletInterface, "Request failed\n");
        // stalling
        needRetry = true;
        return false;
    } else {
        DPRINTF(CodeletInterface, "Request succeeded\n");
        return true;
    }
}

void
CodeletInterface::CPUSidePort::recvRespRetry()
{
    // We should have a blocked packet if this function is called.
    assert(blockedPacket != nullptr);

    // Grab the blocked packet.
    PacketPtr pkt = blockedPacket;
    blockedPacket = nullptr;

    DPRINTF(CodeletInterface, "Retrying response pkt %s\n", pkt->print());
    // Try to resend it. It's possible that it fails again.
    sendPacket(pkt);

    // We may now be able to accept new packets
    trySendRetry();
}

void
CodeletInterface::MemSidePort::sendPacket(PacketPtr pkt)
{
    // Note: This flow control is very simple since the cache is blocking.

    panic_if(blockedPacket != nullptr, "Should never try to send if blocked!");

    // If we can't send the packet across the port, store it for later.
    if (!sendTimingReq(pkt)) {
        blockedPacket = pkt;
    }
}

bool
CodeletInterface::MemSidePort::recvTimingResp(PacketPtr pkt)
{
    // Just forward to the interface.
    return owner->handleResponse(pkt);
}

void
CodeletInterface::MemSidePort::recvReqRetry()
{
    // We should have a blocked packet if this function is called.
    assert(blockedPacket != nullptr);

    // Grab the blocked packet.
    PacketPtr pkt = blockedPacket;
    blockedPacket = nullptr;

    // Try to resend it. It's possible that it fails again.
    sendPacket(pkt);
}

// -----------------------------------<Codelet Interface Funcs>---------------------------------

bool
CodeletInterface::handleRequest(PacketPtr pkt, int port_id)
{
    if (blocked) {
        // There is currently an outstanding request so we can't respond. Stall
        return false;
    }

    DPRINTF(CodeletInterface, "Got request for addr %#x\n", pkt->getAddr());

    // This interface is now blocked waiting for the response to this packet.
    blocked = true;

    // Store the port for when we get the response
    assert(waitingPortId == -1);
    waitingPortId = port_id; // this is valid if the request was from CPU
    // invalid if from codbus (SU)

    // Here we check address to see if a queue interaction is needed
    Addr reqAddr = pkt->getAddr();
    if (queueRange.contains(reqAddr)) {
        // Local queue access
        // Schedule an event after queue access latency to actually access
        schedule(new EventFunctionWrapper([this, pkt]{ accessTiming(pkt); },
                                      name() + ".accessEvent", true),
                clockEdge(queueLatency));
    } else {
        // for now, ignore SU address space -- if not local queue access, forward
        DPRINTF(CodeletInterface, "forwarding packet\n");
        memPort.sendPacket(pkt);
    }
    return true;
}

bool
CodeletInterface::handleResponse(PacketPtr pkt)
{
    assert(blocked);
    DPRINTF(CodeletInterface, "Got response for addr %#x\n", pkt->getAddr());

    // just forward the response back to CPU
    sendResponse(pkt);

    return true;
}


void
CodeletInterface::sendResponse(PacketPtr pkt)
{
    assert(blocked);
    DPRINTF(CodeletInterface, "Sending resp for addr %#x\n", pkt->getAddr());

    int port = waitingPortId; // valid if request was from CPU
    // InvalidPortID if request was from SU

    // The packet is now done. We're about to put it in the port, no need for
    // this object to continue to stall.
    // We need to free the resource before sending the packet in case the CPU
    // tries to send another request immediately (e.g., in the same callchain).
    blocked = false;
    waitingPortId = -1;

    // check if responding to CPU or back to SU
    if (port == InvalidPortID) { // response to SU
        codRespPort.sendPacket(pkt);
    } else {
        // Respond to CPU port that sent the request
        cpuPorts[port].sendPacket(pkt);
    }

    // For each of the cpu ports, if it needs to send a retry, it should do it
    // now since this memory object may be unblocked now.
    // this also needs to trySendRetry the codRespPort
    codRespPort.trySendRetry();
    for (auto& port : cpuPorts) {
        port.trySendRetry();
    }
}
// -----------------------------------<\end Codelet Interface Funcs>--------------------------

// -----------------------------------<Codelet Response Port>---------------------------------
void
CodeletInterface::CodSideRespPort::sendPacket(PacketPtr pkt)
{
    panic_if(blockedPacket != nullptr, "Should never try to send if blocked!");

    // If we can't send the packet across the port, store it for later.
    DPRINTF(CodeletInterfaceCod, "Sending %s to SU\n", pkt->print());
    if (!sendTimingResp(pkt)) {
        DPRINTF(CodeletInterfaceCod, "failed!\n");
        blockedPacket = pkt;
    }
}

void
CodeletInterface::CodSideRespPort::trySendRetry()
{
    if (needRetry && blockedPacket == nullptr) {
        // Only send a retry if the port is now completely free
        needRetry = false;
        DPRINTF(CodeletInterfaceCod, "Sending retry req.\n");
        sendRetryReq();
    }
}

void
CodeletInterface::CodSideRespPort::recvFunctional(PacketPtr pkt)
{
    // Just forward to the interface
    // handleFunctional should decide whether to forward or do a queue operation
    return owner->handleFunctional(pkt);
}

bool
CodeletInterface::CodSideRespPort::recvTimingReq(PacketPtr pkt)
{
    DPRINTF(CodeletInterfaceCod, "Got request %s\n", pkt->print());

    if (blockedPacket || needRetry) {
        // The interface may not be able to send a reply if this is blocked
        DPRINTF(CodeletInterfaceCod, "Request blocked\n");
        needRetry = true;
        return false;
    }
    // Just forward to the interface.
    if (!owner->handleRequest(pkt, InvalidPortID)) {
        DPRINTF(CodeletInterfaceCod, "Request failed\n");
        // stalling
        needRetry = true;
        return false;
    } else {
        DPRINTF(CodeletInterfaceCod, "Request succeeded\n");
        return true;
    }
}

void
CodeletInterface::CodSideRespPort::recvRespRetry()
{
    // We should have a blocked packet if this function is called.
    assert(blockedPacket != nullptr);

    // Grab the blocked packet.
    PacketPtr pkt = blockedPacket;
    blockedPacket = nullptr;

    DPRINTF(CodeletInterfaceCod, "Retrying response pkt %s\n", pkt->print());
    // Try to resend it. It's possible that it fails again.
    sendPacket(pkt);

    // We may now be able to accept new packets
    trySendRetry();
}

// -----------------------------------<\end Codelet Response Port>----------------------------
void
CodeletInterface::MemSidePort::recvRangeChange()
{
    DPRINTF(CodeletInterface, "CodeletInterface receiving range change from MemSidePort\n");
    owner->sendRangeChange();
}


void
CodeletInterface::sendRangeChange() const
{
    DPRINTF(CodeletInterface, "CodeletInterface sending range change to CPU ports\n");
    for (auto& port : cpuPorts) {
        port.sendRangeChange();
    }
}

AddrRangeList
CodeletInterface::CPUSidePort::getAddrRanges() const
{
   DPRINTF(CodeletInterface, "getting new addr ranges for CPU-side ports\n");
    //coming from CPU so return all addr ranges CPU can access through this port
    // i.e. local queue space, SU space, mem space
    AddrRangeList ranges;
    // include SU space accessible from the interface
    ranges.merge(owner->getCodAddrRanges());
    // include local interface queue space
    ranges.merge(owner->getLocAddrRanges());
    // include mem space
    ranges.merge(owner->getMemAddrRanges());
    return(ranges);
}

void
CodeletInterface::init()
{
    codRespPort.sendRangeChange();
}

AddrRangeList
CodeletInterface::CodSideRespPort::getAddrRanges() const
{
    DPRINTF(CodeletInterface, "Sending new codbus addr ranges\n");
    // CodSideRespPort means getAddrRanges is triggered by SU(?)
    // so send the addresses accessible to SU i.e. queue addresses
    return(owner->getLocAddrRanges());
}

AddrRangeList
CodeletInterface::getCodAddrRanges() const
{
    //DPRINTF(CodeletInterface, "getting new codbus addr ranges\n");
    // Just use the same ranges as whatever is on the codbus side (SU).
    // For now merge the two different address ranges (which are actually lists)
    AddrRangeList ranges;
    ranges.merge(codReqPorts[0].getAddrRanges());
    ranges.merge(codReqPorts[1].getAddrRanges());
    return(ranges);
}

AddrRangeList
CodeletInterface::getMemAddrRanges() const
{
    DPRINTF(CodeletInterface, "Sending new membus addr ranges\n");
    // Just use the same ranges as whatever is on the memory side.
    return(memPort.getAddrRanges());
}

AddrRangeList
CodeletInterface::getLocAddrRanges() const
{
    // returns AddrRange representing local codelet queue
    AddrRangeList ranges;
    ranges.push_back(queueRange);
    return(ranges);
}

void
CodeletInterface::CodSideReqPort::recvRangeChange()
{
    //owner->sendRangeChange();
    // for now we leave these empty
}

void
CodeletInterface::sendCodRangeChange() const
{
    // again for now we're going to leave this blank
    // this is a rangeChange coming from SU --
    // the SU telling interface to ask for its address range....
    
    // do we call getAddrRange on the Cod req port in response?
    // Are the port maps automatically assigned somehow?
}

CodeletInterface::CodeletInterfaceStats::CodeletInterfaceStats(statistics::Group *parent)
      : statistics::Group(parent),
      ADD_STAT(cods, statistics::units::Count::get(), "Number of codelets fired"),
      ADD_STAT(sigs, statistics::units::Count::get(), "Number of dependencies signaled"),
      ADD_STAT(codLatency, statistics::units::Tick::get(),
               "Ticks for Codelet latency from pop to finish")
{
    codLatency.init(16); // number of buckets, can change later
}


} // namespace gem5
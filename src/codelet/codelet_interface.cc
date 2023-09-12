
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

#define CODELET_SIZE sizeof(runt_codelet_t) 
CodeletInterface::CodeletInterface(const CodeletInterfaceParams &params) :
    ClockedObject(params),
    queueLatency(params.queue_latency),
    genLatency(params.gen_latency),
    capacity(params.size / CODELET_SIZE),
    queueRange(params.queue_range),
    cuId(params.cu_id),
    suRetAddr(params.su_ret_addr),
    cpuPort(params.name + ".cpu_side_port", this, "CPUSidePort"),
    memPort(params.name + ".mem_side_port", this, "MemSidePort"),
    codReqPort(params.name + "cod_side_req_port", this),
    codRespPort(params.name + ".cod_side_resp_port", this),
    blocked(false), originalPacket(nullptr), waitingPortId(-1), stats(this)
{
    // Since the CPU side ports are a vector of ports, create an instance of
    // the CPUSidePort for each connection. This member of params is
    // automatically created depending on the name of the vector port and
    // holds the number of connections to this port name
    /*
    for (int i = 0; i < params.port_cpu_side_ports_connection_count; ++i) {
        cpuPorts.emplace_back(name() + csprintf(".cpu_side_ports[%d]", i), i, this, "CPUSidePort" + csprintf("[%d]", i));
    }
     */
    // removing second CodSideReqPort -- not needed, avoiding handling port ID
    /*
    for (int i = 0; i < params.port_cod_side_req_ports_connection_count; i++) {
        // cod req ports here. and lets hope i got the name of the param right??
        codReqPorts.emplace_back(name() + csprintf(".cod_side_req_ports[%d]", i), i, this);
    }
     */
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
    } else if (if_name == "cpu_side_port") {
        // We should have already created all of the ports in the constructor
        return(cpuPort);
    } else if (if_name == "cod_side_req_port") {
        panic_if(idx != InvalidPortID,
                 "Cod side req. port of codelet interface not a vector port");
        return(codReqPort);
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
CodeletInterface::handleFunctional(PacketPtr pkt, int port_id)
{
    // Here we check address to see if a queue interaction is needed
    Addr reqAddr = pkt->getAddr();
    if (queueRange.contains(reqAddr)) {
        if(accessFunctional(pkt, port_id)) {
            sendResponse(pkt, port_id);
            return;
        }
        else {
            // big question: what to do if the codelet pop fails? introduce a NullCodelet??
            // ignore for now....
            sendResponse(pkt, port_id);
        }
    }
    // also will have to add routing to CU / codelet ports here later.....
    else {
        memPort.sendFunctional(pkt);
    }
}

bool
CodeletInterface::accessFunctional(PacketPtr pkt, int port_id)
{
    // Read request: return whatever data the CPU is asking for based on the address
    DPRINTF(CodeletInterfaceQueue, "received packet %s and is write: %d, portID = %d\n", pkt->print(), pkt->isWrite(), port_id);
    if (pkt->isRead()) {
        Addr reqAddr = pkt->getAddr(); // the address trying to be read
        auto data = pkt->getPtr<uint8_t>(); // pointer to data field of the packet
        if (reqAddr >= queueRange.start() + sizeof(runt_codelet_t)) { // if requested address is not in the activeCodelet space
            // CPU must be trying to read codeletAvailable flag
            pkt->makeResponse();
            std::memcpy(data, &codeletAvailable, pkt->getSize());
            DPRINTF(CodeletInterfaceQueue, "CPU reading codeletAvailable flag = %x\n", codeletAvailable);
            sendResponse(pkt, port_id); // send back to origin port 
            return(true);
        }
        assert(codeletAvailable != 0); // activeCodelet read should never come when available flag is false
        runt_codelet_t toPop = activeCodelet; //get available codelet
        pkt->makeResponse();
        //Addr codOffset = reqAddr - Addr(0x90000000); // offset of field requested
        Addr codOffset = reqAddr - queueRange.start(); // offset of field requested
        char * activeCodStart = (char *) &activeCodelet;
        char * fieldPtr = activeCodStart + codOffset; // pointer to data field requested
        std::memcpy(data, fieldPtr, pkt->getSize()); //only copy data of size that was requested
        DPRINTF(CodeletInterfaceQueue, "returning field at %lx (based on offset %lx) that has data %lx\n", (unsigned long) fieldPtr, (unsigned long)codOffset, (unsigned long)*((unsigned long *)data));
        DPRINTF(CodeletInterfaceQueue, "activeCodelet is: %lx %p %p %p %s\n", (unsigned long) toPop.fire, toPop.dest, toPop.src1, toPop.src2, toPop.name);
        sendResponse(pkt, port_id); // send back to origin port 
        return(true);
    }
    // InvalidPortID means that this write request is from the SU
    // Meaning it is an attempt to push a Codelet to the CU
    else if (pkt->isWrite() && port_id == InvalidPortID) {
        auto pkt_data = pkt->getPtr<uint8_t>();
        runt_codelet_t toPush;
        std::memcpy(&toPush, pkt_data, sizeof(runt_codelet_t));
        // if no codelet staged, bypass queue, stage immeidately
        // and set available flag
        if (!codeletAvailable) {
            DPRINTF(CodeletInterfaceQueue, "pushing Codelet to active: %lx %p %p %p %s\n", (unsigned long) toPush.fire, toPush.dest, toPush.src1, toPush.src2, toPush.name);
            codeletAvailable = 1;
            activeCodelet = toPush;
        } else { //if already a codelet being used by CU, push this one to the queue
            DPRINTF(CodeletInterfaceQueue, "pushing Codelet to queue: %lx %p %p %p %s\n", (unsigned long) toPush.fire, toPush.dest, toPush.src1, toPush.src2, toPush.name);
            codQueue.push(toPush);
        }
        DPRINTF(CodeletInterfaceQueue, "making response for packet %s", pkt->print());
        pkt->makeResponse(); // with no data modification, because it was a write
        DPRINTF(CodeletInterfaceQueue, "response made: %s\n", pkt->print());
        sendResponse(pkt, port_id); // send back to origin port 
        // if the CPU port is blocked because of this Codelet push, unblock it now that we're done
        /*
        if (cpuPort.isBlocked()) {
            cpuPort.clearBlocked();
        }
        */
        return(true);
    }
    // A valid port ID means this request is from the CPU, and it is a write
    // request so the CPU is retiring a Codelet -- the active Codelet
    // Later we also need to forward this to the SU so fetchDecode can
    // accurately retire the associated instruction, but for now we will just
    // send a response
    // TODO: forward this request to the SU after implementing CodReqPort here
    // and CodRespPort in SU. Maybe flow issues if we forward from here, we'll see
    else if (pkt->isWrite()) {
        // copy active codelet to send to SU so it can retire the associated instruction
        //runt_codelet_t * toRetire = new runt_codelet_t;
        retire_data_t * toRetire = new retire_data_t;
        memcpy(&(toRetire->toRet), &activeCodelet, sizeof(runt_codelet_t));
        toRetire->cuId = cuId; 
        // perform local codelet retirement; stage new codelet or set codeletAvailable to 0
        if (codQueue.empty()) { //if the queue is empty, no more codelets currently available
            DPRINTF(CodeletInterfaceQueue, "CPU retiring Codelet %s with cuId %d; no more Codelets available\n", activeCodelet.name, toRetire->cuId);
            codeletAvailable = 0;
        } else { // if not, simply stage the next codelet from the queue
            runt_codelet_t toPush = codQueue.front();
            activeCodelet = toPush;
            DPRINTF(CodeletInterfaceQueue, "CPU retiring Codelet %s with cuId %d; setting new activeCodelet %s\n", toRetire->toRet.name, toRetire->cuId, toPush.name);
            codQueue.pop();
        }
        // send to SU first -- build new packet / request of size runt_codelet_t for SU
        //Addr addr = pkt->getAddr();
        DPRINTF(CodeletInterfaceQueue, "Upgrading packet to codelet size\n");
        assert(pkt->needsResponse());
        // Save the old packet
        originalPacket = pkt;
        MemCmd cmd;
        cmd = MemCmd::WriteReq;
        // Create a new packet that is size of codelet
        //PacketPtr new_pkt = new Packet(pkt->req, cmd, sizeof(runt_codelet_t));
        PacketPtr new_pkt = new Packet(pkt->req, cmd, sizeof(retire_data_t));
        // i believe this constructor modifies the address inherently to be 
        // block aligned, we will have to make sure that doesn't ruin the address to the SU
        //new_pkt->allocate();
        //auto data = pkt->getPtr<runt_codelet_t>();
        auto data = pkt->getPtr<retire_data_t>();
        unsigned int size = new_pkt->getSize();
        //assert(size == sizeof(runt_codelet_t));
        //memcpy(data, toRetire, sizeof(runt_codelet_t));
        //DPRINTF(CodeletInterfaceQueue, "codelet toRetire check");
        new_pkt->setAddr(suRetAddr);
        //new_pkt->dataDynamic<runt_codelet_t>(toRetire);
        new_pkt->dataDynamic<retire_data_t>(toRetire);
        codReqPort.sendPacket(new_pkt); //send new packet to SU
        // do not send response here: the response will be handled when the CodeletInterface receives
        // the response from the SU
        return(true);
    }
    else {
        return(false);
    }
}

void
CodeletInterface::accessTiming(PacketPtr pkt, int port_id)
{
    // first do the access logic and check if it worked
    if (!accessFunctional(pkt, port_id)) {
        // if didn't work:
        // send response without changing; runtime should check for null response
        pkt->makeResponse();
        auto data = pkt->getPtr<uint8_t>();
        auto size = pkt->getSize();
        std::fill(data, data + size, 0); //send nullptr basically
        sendResponse(pkt, port_id);
    }
}

/*
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
    return owner->handleFunctional(pkt, 0);
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
*/

// codelet interface should never snoop so we might be able to remove this
/*
bool
CodeletInterface::CPUSidePort::recvTimingSnoopResp(PacketPtr pkt)
{

    assert(pkt->isResponse());

    // Express snoop responses from requestor to responder, e.g., from L1 to L2
    cache->recvTimingSnoopResp(pkt);
    return true;
}
*/
void
CodeletInterface::CIResponsePort::setBlocked()
{
    assert(!blocked);
    DPRINTF(CodeletInterface, "CPUSidePort is blocking new requests\n");
    blocked = true;
    // if we already scheduled a retry in this cycle, but it has not yet
    // happened, cancel it
    if (sendRetryEvent.scheduled()) {
        owner.deschedule(sendRetryEvent);
        DPRINTF(CodeletInterface, "CPUSidePort descheduled retry\n");
        mustSendRetry = true;
    }
}

void
CodeletInterface::CIResponsePort::clearBlocked()
{
    assert(blocked);
    DPRINTF(CodeletInterface, "CPUSidePort is accepting new requests\n");
    blocked = false;
    if (mustSendRetry) {
        // @TODO: need to find a better time (next cycle?)
        owner.schedule(sendRetryEvent, curTick() + 1);
    }
}

void
CodeletInterface::CIResponsePort::processSendRetry()
{
    DPRINTF(CodeletInterface, "Response Port is sending retry\n");

    // reset the flag and call retry
    mustSendRetry = false;
    sendRetryReq();
}

bool
CodeletInterface::CPUSidePort::tryTiming(PacketPtr pkt)
{
    /* // no snooping, should never be bypassed
    if (cache->system->bypassCaches() || pkt->isExpressSnoop()) {
        // always let express snoop packets through even if blocked
        return true;
    } else if (blocked || mustSendRetry) {
        // either already committed to send a retry, or blocked
        mustSendRetry = true;
        return false;
    }
    */
   // requests from the CPU only need to be blocked if it is trying to do codelet retirement 
   // because codelet retirement involves operations on the queue instead of reading from activeCodelet
   // all other requests can proceed
   if (ci->queueRange.contains(pkt->getAddr()) && pkt->isWrite() && (blocked || mustSendRetry)) {
        // either already committed to send a retry, or blocked
        mustSendRetry = true;
        return false;
    }

    mustSendRetry = false;
    return true;
}

bool
CodeletInterface::CPUSidePort::recvTimingReq(PacketPtr pkt)
{
    assert(pkt->isRequest());
    /*
    if (cache->system->bypassCaches()) {
        // Just forward the packet if caches are disabled.
        // @todo This should really enqueue the packet rather
        [[maybe_unused]] bool success = cache->memSidePort.sendTimingReq(pkt);
        assert(success);
        return true;
    } else if (tryTiming(pkt)) {
        cache->recvTimingReq(pkt);
        return true;
    }
    */
    if (!ci->queueRange.contains(pkt->getAddr())) {
        ci->memPort.schedTimingReq(pkt, curTick() + 1);
        return(true);
    }
    if (tryTiming(pkt)) {
        //ci->recvTimingReq(pkt);
        ci->handleRequest(pkt, 0);
        return true;
    }
    return false;
}

/*
Tick
CodeletInterface::CPUSidePort::recvAtomic(PacketPtr pkt)
{
    return ci->recvAtomic(pkt);
}
*/

void
CodeletInterface::CPUSidePort::recvFunctional(PacketPtr pkt)
{
    // functional request
    // shouldn't be called since we always run in timing mode
    // but keep in mind for errors
    DPRINTF(CodeletInterface, "Received functional request %s from CPU\n", pkt->print());
    ci->handleFunctional(pkt, 0);
}

CodeletInterface::CPUSidePort::CPUSidePort(const std::string &_name, CodeletInterface *_ci,
                         const std::string &_label)
    : CIResponsePort(_name, _ci, _label), ci(_ci)
{ }
/*
CodeletInterface::CPUSidePort::CPUSidePort(const std::string &_name,
                                    CodeletInterface *_ci,
                                    const std::string &_label)
    : CIResponsePort(_name, _ci, _label)
    { }
*/

CodeletInterface::CIResponsePort::CIResponsePort(const std::string &_name,
                                          CodeletInterface *_ci,
                                          const std::string &_label)
    : QueuedResponsePort(_name, _ci, queue),
      queue(*_ci, *this, true, _label),
      blocked(false), mustSendRetry(false),
      sendRetryEvent([this]{ processSendRetry(); }, _name)
{
}

// -----------------------------------------<CodSideReqPort Functions -----------------------------
void
CodeletInterface::CodSideReqPort::sendPacket(PacketPtr pkt)
{
    // Note: This flow control is very simple since the cache is blocking.

    panic_if(blockedPacket != nullptr, "Should never try to send if blocked!");

    // If we can't send the packet across the port, store it for later.
    if (!sendTimingReq(pkt)) {
        blockedPacket = pkt;
    }
}

bool
CodeletInterface::CodSideReqPort::recvTimingResp(PacketPtr pkt)
{
    // Just forward to the interface.
    return owner->handleResponse(pkt);
}

void
CodeletInterface::CodSideReqPort::recvReqRetry()
{
    // We should have a blocked packet if this function is called.
    assert(blockedPacket != nullptr);

    // Grab the blocked packet.
    PacketPtr pkt = blockedPacket;
    blockedPacket = nullptr;

    // Try to resend it. It's possible that it fails again.
    sendPacket(pkt);
}
// ------------------------------------<\end CodSideReqPort Functions --------------------------

// -----------------------------------------<MemSidePort Functions -----------------------------
/*
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
*/

bool
CodeletInterface::MemSidePort::recvTimingResp(PacketPtr pkt)
{
    ci->recvMemTimingResp(pkt);
    return true;
}

void
CodeletInterface::recvMemTimingResp(PacketPtr pkt)
{
    DPRINTF(CodeletInterface, "Got response for addr %#x\n", pkt->getAddr());
    // just forward the response back to CPU     
    // i think now that its queued port we don't need original packet
    /*
    if (originalPacket != nullptr) {
        DPRINTF(CodeletInterface, "Response from SU; getting original packet to reply to CPU\n");
        // We had to upgrade a previous packet. We can functionally deal with
        // the cache access now. It better be a hit.
        originalPacket->makeResponse();
        delete pkt; // We may need to delay this, I'm not sure.
        pkt = originalPacket;
        originalPacket = nullptr;
    } // else, pkt contains the data it needs
     */
    //sendResponse(pkt);
    // we know that this response came from memory which means it can only be
    // a response that is heading towards the CPU so send it directly
    //cpuPort.sendTimingResp(pkt);
    cpuPort.schedTimingResp(pkt, curTick() + 1);

}

/*
void
CodeletInterface::CIReqPacketQueue::sendDeferredPacket()
{
    // sanity check
    assert(!waitingOnRetry);

    // there should never be any deferred request packets in the
    // queue, instead we resly on the cache to provide the packets
    // from the MSHR queue or write queue ????
    assert(deferredPacketReadyTime() == MaxTick);

    // check for request packets 
    PacketPtr toSend = ci->requestQueue.front();
    ci->requestQueue.pop();
    waitingOnRetry = entry->sendPacket(cache);

    // if we succeeded and are not waiting for a retry, schedule the
    // next send considering when the next queue is ready, note that
    // snoop responses have their own packet queue and thus schedule
    // their own events
    if (!waitingOnRetry) {
        schedSendEvent(ci.nextQueueReadyTime());
    }
}
*/

CodeletInterface::MemSidePort::MemSidePort(const std::string &_name,
                                    CodeletInterface *_ci,
                                    const std::string &_label)
    : CIRequestPort(_name, _ci, _reqQueue, _snoopRespQueue),
      _reqQueue(*_ci, *this, _snoopRespQueue, _label),
      _snoopRespQueue(*_ci, *this, true, _label), ci(_ci)
{
}

// ------------------------------------<\end MemSidePort Functions -----------------------------


// -----------------------------------<Codelet Interface Funcs>---------------------------------

bool
CodeletInterface::handleRequest(PacketPtr pkt, int port_id)
{
    if (blocked) {
        // There is currently an outstanding request so we can't respond. Stall
        return false;
    }

    DPRINTF(CodeletInterface, "Got request %s\n", pkt->print());

    // This interface is now blocked waiting for the response to this packet.
    //blocked = true;

    // Store the port for when we get the response
    //assert(waitingPortId == -1);
    waitingPortId = port_id; // this is valid if the request was from CPU
    // invalid if from codbus (SU)

    // Here we check address to see if a queue interaction is needed
    Addr reqAddr = pkt->getAddr();
    if (queueRange.contains(reqAddr)) {
        // Local queue access
        // Schedule an event after queue access latency to actually access
        schedule(new EventFunctionWrapper([this, pkt, port_id]{ accessTiming(pkt, port_id); },
                                      name() + ".accessEvent", true),
                clockEdge(queueLatency));
    } else {
        // for now, ignore SU address space -- if not local queue access, forward
        DPRINTF(CodeletInterface, "forwarding packet\n");
        memPort.schedTimingReq(pkt, curTick() + 1);
    }
    return true;
}

bool
CodeletInterface::handleResponse(PacketPtr pkt)
{
    //assert(blocked);
    DPRINTF(CodeletInterface, "Got non-mem response for addr %#x\n", pkt->getAddr());
    // just forward the response back to CPU/SU depending on where the 
    // request originated (based on waitingPortId)
    // If we had to upgrade the request packet to a full codelet, now we
    // can use that packet to construct the response.
    if (originalPacket != nullptr) {
        DPRINTF(CodeletInterface, "Response from SU; getting original packet to reply to CPU\n");
        // We had to upgrade a previous packet. We can functionally deal with
        // the cache access now. It better be a hit.
        originalPacket->makeResponse();
        delete pkt; // We may need to delay this, I'm not sure.
        pkt = originalPacket;
        originalPacket = nullptr;
        sendResponse(pkt, 0); // port ID of 0 means CPU port
        return(true);
    } // else, pkt contains the data it needs
    // this function has be bypassed now for memory responses, so this should only actually be for
    // the Codelet Bus, meaning we can safely plug in InvalidPortId as the port ID
    // (the cpu port manually assigns port id as 0 instead of invalid)

    return(false);
}


void
CodeletInterface::sendResponse(PacketPtr pkt, int port_id)
{
    //assert(blocked);
    DPRINTF(CodeletInterface, "Sending resp for addr %#x\n", pkt->getAddr());

    int port = port_id; // valid if request was from CPU
    // InvalidPortID if request was from SU

    // The packet is now done. We're about to put it in the port, no need for
    // this object to continue to stall.
    // We need to free the resource before sending the packet in case the CPU
    // tries to send another request immediately (e.g., in the same callchain).
    blocked = false;
    //waitingPortId = -1;

    // check if responding to CPU or back to SU
    if (port == InvalidPortID) { // response to SU
        codRespPort.sendPacket(pkt);
        // this also needs to trySendRetry the codRespPort
        codRespPort.trySendRetry();
    } else {
        // Respond to CPU port that sent the request
        //cpuPort.sendTimingResp(pkt);
        cpuPort.schedTimingResp(pkt, curTick() + 1);
    }

    /*
    for (auto& port : cpuPorts) {
        port.trySendRetry();
    }
    */
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
    // handleFunctional can't actually correctly handle a Codelet request right now
    // but the SU shouldn't ever send a functional request, so let's ignore that
    return owner->handleFunctional(pkt, InvalidPortID);
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
        /*
        if (owner->queueRange.contains(pkt->getAddr()) && pkt->isWrite()) {
            // if pushing new codelet to queue, need to block incoming requests from CPU
            // so that there is no queue manipulation conflict / race conditions
            owner->cpuPort.setBlocked();
        }
        */
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
    ci->sendRangeChange();
}


void
CodeletInterface::sendRangeChange() const
{
    DPRINTF(CodeletInterface, "CodeletInterface sending range change to CPU ports\n");
    cpuPort.sendRangeChange();
}

AddrRangeList
CodeletInterface::CPUSidePort::getAddrRanges() const
{
   DPRINTF(CodeletInterface, "getting new addr ranges for CPU-side ports\n");
    //coming from CPU so return all addr ranges CPU can access through this port
    // i.e. local queue space, SU space, mem space
    AddrRangeList ranges;
    // include SU space accessible from the interface
    ranges.merge(ci->getCodAddrRanges());
    // include local interface queue space
    ranges.merge(ci->getLocAddrRanges());
    // include mem space
    ranges.merge(ci->getMemAddrRanges());
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
    ranges.merge(codReqPort.getAddrRanges());
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
    DPRINTF(CodeletInterfaceQueue, "CodeletInterface has range 0x%lx - 0x%lx\n", queueRange.start(), queueRange.end());
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
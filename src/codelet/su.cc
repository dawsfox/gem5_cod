
#include "debug/SU.hh"
#include "debug/SULoader.hh"
#include "debug/SUCod.hh"
#include "sim/system.hh"
#include "sim/process.hh"
#include "base/loader/elf_object.hh"
#include "mem/packet.hh"
#include "codelet/su.hh"
#include "codelet/codelet.hh"
#include "gelf.h"
#include <string.h>
#include <cstring>

namespace gem5
{

void
SU::getCodelets()
{
    ThreadContext *tmp_context = system->threads[0];
    loader::ObjectFile *tmp_obj = tmp_context->getProcessPtr()->objFile;
    loader::ElfObject *elfObject = dynamic_cast<loader::ElfObject *>(tmp_obj);
    //DPRINTF(SULoader, "elfObject has address %p\n", elfObject);
    Elf * real_elf = elfObject->getElf();
    //DPRINTF(SULoader, "real elf has address %p\n", real_elf);
    // we added getElf so we can go grab the Codelet section
    size_t string_index;
    // get index of string table so we can find section names
    elf_getshdrstrndx(real_elf, &string_index);
    //DPRINTF(SULoader, "string table is at section %u\n", string_index);
    Elf_Scn *section = elf_getscn(real_elf, 1);
    //DPRINTF(SULoader, "first section is at %p\n", section);
    bool program_found = false;
    for (int sec_idx = 1; section; section = elf_getscn(real_elf, ++sec_idx)) {
        GElf_Shdr shdr;
        gelf_getshdr(section, &shdr);
        // sh_name is the index in the string table where name is located, so check it in string table
        // Return pointer to string at OFFSET in section INDEX.
        char * section_name = elf_strptr(real_elf, string_index, shdr.sh_name);
        //DPRINTF(SULoader, "checking section %s\n", section_name);
        // if section name is .codelet_program
        if (!strcmp(section_name, ".codelet_program")) {
            DPRINTF(SULoader, "section %s FOUND\n", section_name);
            program_found = true;
            break;
        }    
    }
    if (program_found) {
        // actually copy over the codelets now
        Elf_Data *prog_data = elf_getdata(section, NULL);
        //DPRINTF(SULoader, "Codelet data located at %p\n", prog_data);
        user_codelet_t * codelet_list = (user_codelet_t *) prog_data->d_buf;
        size_t codelet_count = prog_data->d_size / sizeof(user_codelet_t);
        DPRINTF(SULoader, "Codelet list located at %p with %u codelets\n", codelet_list, codelet_count);
        DPRINTF(SULoader, "size of user codelet: %u\n", sizeof(user_codelet_t));
        for (int i=0; i<codelet_count; i++) {
            codelet_t *localCod = (codelet_t *) malloc(sizeof(codelet_t));
            //memcpy(localCod, &(codelet_program[i]), sizeof(codelet_t));
            strcpy(localCod->name, codelet_list[i].name);
            localCod->fire = codelet_list[i].fire;
            DPRINTF(SULoader, "codelet %d is at address %p in elf\n", i, &(codelet_list[i]));
            DPRINTF(SULoader, "real codelet is: %p - %s\n", (void *)localCod->fire, localCod->name);
            //DPRINTF(SULoader, "real codelet is: %p %x %x %x %x\n", (void *)localCod->fire, localCod->dest, localCod->src1, localCod->src2, localCod->id);
            //codQueue.push(*localCod);
            codSpace[i] = *localCod;
            free(localCod);
        }
        /*
        //for test printing
        long unsigned * printable_program = (long unsigned *) prog_data->d_buf;
        size_t byte_size = prog_data->d_size;
        for (int i=0; i<byte_size/8; i++) {
            DPRINTF(SULoader, "%lx\n", printable_program[i]);
        }
        */ 
    }

}

void
SU::getRegs(std::string progLine)
{

}

void
SU::analyzeProgram()
{

}

/*
void
SU::scheduleRequestExt(codelet_t * toSend, Addr dest, Cycles latency)
{
    schedule(new EventFunctionWrapper([this, toSend, dest]{ sendRequestExt(toSend, dest); },
                                    name() + ".pushCodeletEvent", true),
                    clockEdge(latency));

}

bool
SU::sendRequestExt(codelet_t * toSend, Addr dest)
{
    DPRINTF(SUCod, "push Codelet to interface\n");
    return(sendRequest(toSend, dest));
}
*/

// -------------------------------------------------------------------------------------------

void
SU::CodSideRespPort::sendPacket(PacketPtr pkt)
{
    panic_if(blockedPacket != nullptr, "Should never try to send if blocked!");

    // If we can't send the packet across the port, store it for later.
    DPRINTF(SU, "Sending %s to CPU\n", pkt->print());
    if (!sendTimingResp(pkt)) {
        DPRINTF(SU, "failed!\n");
        blockedPacket = pkt;
    }
}

void
SU::CodSideRespPort::trySendRetry()
{
    if (needRetry && blockedPacket == nullptr) {
        // Only send a retry if the port is now completely free
        needRetry = false;
        DPRINTF(SU, "Sending retry req.\n");
        sendRetryReq();
    }
}

void
SU::CodSideRespPort::recvFunctional(PacketPtr pkt)
{
    // Just forward to the interface
    // handleFunctional should decide whether to forward or do a queue operation
    return owner->handleFunctional(pkt);
}


void
SU::handleFunctional(PacketPtr pkt)
{
    // Here we should check address space to implment Codelet Retirement
    // But don't worry about this yet
    Addr reqAddr = pkt->getAddr();
    /*
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
    */
}

bool
SU::accessFunctional(PacketPtr pkt)
{
    // this should be moved somewhere else; accessFunctional will probably be called
    // when the SU pushes to the queue as well. it's fine for now...
    panic_if(!pkt->isRead(), "CPU should not do anything to queue space but read it");
    // pop codelet and return
    if (!codQueue.empty()) {
        codelet_t toPop = codQueue.front(); //get next Codelet up
        codQueue.pop(); // remove it from the queue
        // how to put it in a packet?
        pkt->makeResponse();
        auto data = pkt->getPtr<uint8_t>();
        //auto size = pkt->getSize();
        std::memcpy(data, &(toPop.fire), sizeof(fire_t));
        //pkt->setSize(sizeof(codelet_t));
        //pkt->setSize(sizeof(fire_t));
        // no byte swapping, set new data
        DPRINTF(SU, "popping Codelet from queue to send\n");
        //pkt->dataStatic<codelet_t>(&toPop);
        //pkt->dataStatic<fire_t>(&(toPop.fire));
        //pkt->dataDynamic<fire_t>(&(toPop.fire));
        return(true);
    }
    else {
        return(false);
    }

}

void
SU::accessTiming(PacketPtr pkt)
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
SU::CodSideRespPort::recvTimingReq(PacketPtr pkt)
{
    DPRINTF(SU, "Got request %s\n", pkt->print());

    if (blockedPacket || needRetry) {
        // The interface may not be able to send a reply if this is blocked
        DPRINTF(SU, "Request blocked\n");
        needRetry = true;
        return false;
    }
    // Just forward to the interface.
    if (!owner->handleRequest(pkt, id)) {
        DPRINTF(SU, "Request failed\n");
        // stalling
        needRetry = true;
        return false;
    } else {
        DPRINTF(SU, "Request succeeded\n");
        return true;
    }
}

void
SU::CodSideRespPort::recvRespRetry()
{
    // We should have a blocked packet if this function is called.
    assert(blockedPacket != nullptr);

    // Grab the blocked packet.
    PacketPtr pkt = blockedPacket;
    blockedPacket = nullptr;

    DPRINTF(SU, "Retrying response pkt %s\n", pkt->print());
    // Try to resend it. It's possible that it fails again.
    sendPacket(pkt);

    // We may now be able to accept new packets
    trySendRetry();
}

// have to edit this to cause the effect (signal/retirement) and make response
bool
SU::handleRequest(PacketPtr pkt, int port_id)
{
    if (blocked) {
        // There is currently an outstanding request so we can't respond. Stall
        return false;
    }

    DPRINTF(SU, "Got request for addr %#x\n", pkt->getAddr());

    // This interface is now blocked waiting for the response to this packet.
    blocked = true;

    // Store the port for when we get the response
    assert(waitingPortId == -1);
    waitingPortId = port_id;

    // Here we check address to see if a queue interaction is needed
    Addr reqAddr = pkt->getAddr();
    if (suSigRange.contains(reqAddr)) {
        // Local queue access
        // Schedule an event after queue access latency to actually access
        schedule(new EventFunctionWrapper([this, pkt]{ accessTiming(pkt); },
                                      name() + ".accessEvent", true),
                clockEdge(sigLatency));
    } else {
        DPRINTF(SU, "forwarding packet\n");
        //memPort.sendPacket(pkt);
    }
    return true;
}

bool
SU::handleResponse(PacketPtr pkt)
{
    assert(reqBlocked);
    DPRINTF(SU, "Got response for addr %#x\n", pkt->getAddr());
    
    // called when response received on CodSideReqPort, meaning Codelet is done being pushed...
    // just release block
    reqBlocked = false;

    // for now, hardcode immediately scheduling a new codelet push
    /*
    if (codQueue.size() > 0) {
        Addr interface_addr(0x90000000);
        Cycles latency_scale(2000); //hardcoded hopefully high enough to not be blocked
        codelet_t * toSend = (codelet_t *) malloc(sizeof(codelet_t));
        *toSend = codQueue.front();
        codQueue.pop();
        scheduleRequestExt(toSend, interface_addr, sigLatency);
    }
     */

    return true;
}


void
SU::sendResponse(PacketPtr pkt)
{
    assert(blocked);
    DPRINTF(SU, "Sending resp for addr %#x\n", pkt->getAddr());

    int port = waitingPortId;

    // The packet is now done. We're about to put it in the port, no need for
    // this object to continue to stall.
    // We need to free the resource before sending the packet in case the CPU
    // tries to send another request immediately (e.g., in the same callchain).
    blocked = false;
    waitingPortId = -1;

    // Simply forward to the memory port
    codRespPorts[port].sendPacket(pkt);

    // For each of the cpu ports, if it needs to send a retry, it should do it
    // now since this memory object may be unblocked now.
    for (auto& port : codRespPorts) {
        port.trySendRetry();
    }
}

void
SU::CodSideReqPort::sendPacket(PacketPtr pkt)
{
    // Note: i guess we have to keep this blocking

    panic_if(blockedPacket != nullptr, "Should never try to send if blocked!");

    // If we can't send the packet across the port, store it for later.
    if (!sendTimingReq(pkt)) { // func is inherited
        blockedPacket = pkt;
    }
}

bool
SU::CodSideReqPort::recvTimingResp(PacketPtr pkt)
{
    // Just forward to the interface.
    return owner->handleResponse(pkt);
}

void
SU::CodSideReqPort::recvReqRetry()
{
    // We should have a blocked packet if this function is called.
    assert(blockedPacket != nullptr);

    // Grab the blocked packet.
    PacketPtr pkt = blockedPacket;
    blockedPacket = nullptr;

    // Try to resend it. It's possible that it fails again.
    sendPacket(pkt);
}


bool
SU::sendRequest(codelet_t *toPush, Addr dest)
{
    DPRINTF(SU, "Pushing codelet to interface with addr %#x\n", dest);
    if (reqBlocked) { //can't send if already being used
        return false;
    }
    reqBlocked = true;
    // addr should be one that is contained within the CodeletInterface
    // between 0x90000000 and 0x9000000f. Should be a write, also...

    // need to make a request object first to pass to the packet
    // let's say the SU has requestor ID 55..... if not, make it invalid somehow
    Request::Flags reqFlags(Request::UNCACHEABLE | Request::PHYSICAL);
    const RequestPtr codReq = std::shared_ptr<Request>(new Request(dest, sizeof(codelet_t), reqFlags, 55));
    PacketPtr codPkt = Packet::createWrite(codReq);
    codPkt->dataStatic<codelet_t>(toPush);
    codReqPort.sendPacket(codPkt);
    return(true);
}

// -------------------------------------------------------------------------

void
SU::CodSideReqPort::recvRangeChange()
{
    DPRINTF(SU, "SU receiving range change\n");
    // indicates range change coming from CodeletInterface
    //owner->sendRangeChange();
}

void
SU::sendRangeChange() const
{
    DPRINTF(SU, "SU sending range change\n");
    // call on the SU's response side ports...
    for (auto& port : codRespPorts) {
        port.sendRangeChange();
    }

}

AddrRangeList
SU::CodSideRespPort::getAddrRanges() const
{ 
    DPRINTF(SU, "getting addr ranges for cod-side resp. ports\n");
    AddrRangeList ranges;
    /// CodSideRespPort has a local AddrRange suRange
    // it is set based on ID
    ranges.push_back(suRange);
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

void
SU::init()
{
    sendRangeChange();    
    getCodelets();
    analyzeProgram();
}

void
SU::startup()
{
    // here: schedule first tick
    DPRINTF(SU, "scheduling first tick\n");
    schedule(tickEvent, clockEdge(Cycles(1)));
}

void
SU::tick()
{
    //DPRINTF(SU, "SU ticking\n");
    // each tick should check first if there are codelets
    // in the codelet space that should go to the queue
    /* no local codelet space setup yet so lets ignore first part */
    // then should push out codelets from the queue -- one per tick?
    if (!codQueue.empty() && !reqBlocked) {
        Addr interface_addr(0x90000000);
        codelet_t * localCod = (codelet_t *) malloc(sizeof(codelet_t));
        *localCod = codQueue.front();
        codQueue.pop();
        sendRequest(localCod, interface_addr);
    }
    else if (!reqBlocked) { // if queue empty but requests aren't blocked
        // later, this should be wrapped in a private function
        // for now we are saying if no codelets left, end
        // later it will be based on deps / program flow
        Addr interface_addr(0x90000000);
        sendRequest(&finalCod, interface_addr);
        aliveSig = false;
    }
    if (aliveSig) {
        schedule(tickEvent, clockEdge(Cycles(1)));
    }
}

SU::SU(const SUParams &params) :
    ClockedObject(params),
    controlStore(1),
    instructionMem(params.scm_file_name.data(), &regFile),
    ilpMode(scm::SEQUENTIAL),
    fetchDecode(&instructionMem, &controlStore, &aliveSig, ilpMode, this),
    scmFileName(params.scm_file_name.data()),
    tickEvent([this]{ tick(); }, "SU tick",
                false, Event::CPU_Tick_Pri),
    aliveSig(true),
    system(params.system),
    sigLatency(params.sig_latency),
    capacity(params.size), //should make this more accurate later, right now it doesn't matter
    suSigRange(params.su_sig_range),
    suRetRange(params.su_ret_range),
    codReqPort(params.name + ".cod_side_req_port", this),
    blocked(false), reqBlocked(false), originalPacket(nullptr), 
    waitingPortId(-1), stats(this)
{
    //strcpy(scmFileName, params.scm_file_name.c_str());
    //instructionMem(scmFileName, &regFile);
    for (int i = 0; i < params.port_cod_side_resp_ports_connection_count; i++) {
        // again unsure of the param name ...
        codRespPorts.emplace_back(name() + csprintf(".cod_side_resp_ports[%d]", i), i, this);
    }
    DPRINTF(SULoader, "SCM Program loaded from %s", scmFileName);
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

} // namespace gem5
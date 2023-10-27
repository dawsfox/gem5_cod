
#include "debug/SU.hh"
#include "debug/SULoader.hh"
#include "debug/SUCod.hh"
#include "debug/SUSCM.hh"
#include "debug/SUMem.hh"
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

unsigned
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
        // -2 because the last two codelets are unique;
        // the first one is a special init codelet with a unique name that always stalls completely to perform initializations
        // the second one is a dummy codelet that holds the base pointer of SCM memory 
        for (int i=0; i<codelet_count-2; i++) {
            DPRINTF(SULoader, "codelet %d is at address %p in elf\n", i, &(codelet_list[i]));
            DPRINTF(SULoader, "codelet is: %x - %p - %s\n", codelet_list[i].io, (void *)codelet_list[i].fire, codelet_list[i].name);
            char codName[30];
            strcpy(codName, codelet_list[i].name);
            std::string codStringName(codName);
            // copying name from elf and then mapping it to the fire function in the SU::codMapping
            //codMapping[codStringName] = (fire_t) codelet_list[i].fire;
            user_codelet_t tmp;
            tmp.io = codelet_list[i].io;
            strcpy(tmp.name, codelet_list[i].name);
            tmp.fire = codelet_list[i].fire;
            codMapping[codStringName] = tmp;
            DPRINTF(SULoader, "loaded user codelet mapping: %s - %p, %x\n", codStringName.data(), (void *)codMapping[codStringName].fire, codMapping[codStringName].io);
        }
            char codName[30];
            strcpy(codName, codelet_list[codelet_count-2].name);
            std::string codStringName(codName);
            // Loading special initCod
            user_codelet_t tmp;
            tmp.io = codelet_list[codelet_count-2].io;
            strcpy(tmp.name, codelet_list[codelet_count-2].name);
            tmp.fire = codelet_list[codelet_count-2].fire;
            codMapping[codStringName] = tmp;
            DPRINTF(SULoader, "loaded user codelet mapping: %s - %p, %x\n", codStringName.data(), (void *)codMapping[codStringName].fire, codMapping[codStringName].io);
            scmBasePtr = (uint64_t) codelet_list[codelet_count-1].fire;
            DPRINTF(SULoader, "Loaded SCM base pointer: 0x%lx\n", scmBasePtr);
        //return(codelet_count);
    } else { return(0); } // should really put an error here: it should never not find codelets
    // now, find the memory codelets
    program_found = false;
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
        user_memcod_t * memcod_list = (user_memcod_t *) prog_data->d_buf;
        size_t codelet_count = prog_data->d_size / sizeof(user_memcod_t);
        DPRINTF(SULoader, "Codelet list located at %p with %u codelets\n", memcod_list, codelet_count);
        DPRINTF(SULoader, "size of user codelet: %u\n", sizeof(user_codelet_t));
        for (int i=0; i<codelet_count; i++) {
            DPRINTF(SULoader, "codelet %d is at address %p in elf\n", i, &(memcod_list[i]));
            DPRINTF(SULoader, "codelet is: %x - %p - %p - %s\n", memcod_list[i].io, (void *)memcod_list[i].fire, (void *)memcod_list[i].rng_res, memcod_list[i].name);
            char codName[30];
            strcpy(codName, memcod_list[i].name);
            std::string codStringName(codName);
            // copying name from elf and then mapping it to the fire function in the SU::codMapping
            //codMapping[codStringName] = (fire_t) codelet_list[i].fire;
            user_memcod_t tmp;
            tmp.io = memcod_list[i].io;
            strcpy(tmp.name, memcod_list[i].name);
            tmp.fire = memcod_list[i].fire;
            tmp.rng_res = memcod_list[i].rng_res;
            memcodMapping[codStringName] = tmp;
            DPRINTF(SULoader, "loaded user codelet mapping: %s - %p - %p, %x\n", codStringName.data(), (void *)memcodMapping[codStringName].fire, (void *)memcodMapping[codStringName].rng_res, memcodMapping[codStringName].io);
        }
        //return(codelet_count);
    } else { return(0); } // should really put an error here: it should never not find codelets
    return(0);
}

bool
SU::isMemcod(std::string codName)
{
   auto tmp = codMapping.find(codName);
   if (tmp == codMapping.end()) {
        auto mem_tmp = memcodMapping.find(codName);
        if (mem_tmp == memcodMapping.end()) {
            SCMULATE_ERROR(0, "Codelet could not be found in codMapping nor memcodMapping");
        } else { return(true); }
   } else { return(false); }
}

fire_t
SU::getCodeletFire(std::string codName)
{
    fire_t tmp = codMapping[codName].fire;
    if (!tmp) {
      for(auto it = codMapping.cbegin(); it != codMapping.cend(); ++it)
      {
        SCMULATE_INFOMSG(3, "Codelet %s at %p", it->first.c_str(), (void *) it->second.fire);
      }
        SCMULATE_ERROR(0, "Fire function requested from SU for codelet %s not found", codName.c_str());
    }
    return(tmp);
}

fire_t
SU::getMemcodResRng(std::string codName)
{
    fire_t tmp = memcodMapping[codName].rng_res;
    if (!tmp) {
      for(auto it = memcodMapping.cbegin(); it != memcodMapping.cend(); ++it)
      {
        SCMULATE_INFOMSG(3, "Codelet %s with %p", it->first.c_str(), (void *) it->second.rng_res);
      }
        SCMULATE_ERROR(0, "Fire function requested from SU for codelet %s not found", codName.c_str());
    }
    return(tmp);
}

fire_t
SU::getMemcodFire(std::string codName)
{
    fire_t tmp = memcodMapping[codName].fire;
    if (!tmp) {
      for(auto it = memcodMapping.cbegin(); it != memcodMapping.cend(); ++it)
      {
        SCMULATE_INFOMSG(3, "Codelet %s with %p", it->first.c_str(), (void *) it->second.fire);
      }
        SCMULATE_ERROR(0, "Fire function requested from SU for codelet %s not found", codName.c_str());
    }
    return(tmp);
}

uint16_t
SU::getCodeletIo(std::string codName)
{
    // below causes problems because I/O can be 0 and valid
    //uint16_t tmp = codMapping[codName].io;
    auto it = codMapping.find(codName);
    if (it == codMapping.end()) {
      for(auto it = codMapping.cbegin(); it != codMapping.cend(); ++it)
      {
        SCMULATE_INFOMSG(3, "Codelet %s with I/O %x", it->first.c_str(), it->second.io);
      }
        SCMULATE_ERROR(0, "I/O requested from SU for codelet %s not found", codName.c_str());
    }
    return(codMapping[codName].io);
}

unsigned char *
SU::readRegSpacePtr()
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
        if (!strcmp(section_name, ".register_space_ptr")) {
            DPRINTF(SULoader, "section %s FOUND\n", section_name);
            program_found = true;
            break;
        }    
    }
    if (program_found) {
        Elf_Data *prog_data = elf_getdata(section, NULL);
        unsigned char ** reg_space_root =  (unsigned char **) prog_data->d_buf;
        unsigned char * reg_space_root_ptr = reg_space_root[0];
        //DPRINTF(SULoader, "Register space root located at %p; points to runtime space %p\n", prog_data->d_buf, reg_space_root_ptr);
        // program REALLY hates printing reg_space_root_ptr as %p for some reason. it breaks the printing....
        DPRINTF(SULoader, "register space starts at: %x\n", (long unsigned)reg_space_root_ptr);
        // attempting to map register space for the CU processes ... it didn't work because reg space isn't aligned to a page boundary
        /*
        long unsigned reg_space_start = (long unsigned) reg_space_root_ptr;
        for (int i=0; i < numCus; i++) {
            Process * process_ptr = system->threads[i]->getProcessPtr();
            process_ptr->map(Addr(reg_space_start), Addr(reg_space_start), REG_FILE_SIZE_KB * 1000);
        }
         */
        return(reg_space_root_ptr);
    }
    return(nullptr);

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
    panic_if(!pkt->isWrite(), "CodeletInterface should only request retirement from SU i.e. a write request");
    panic_if(executingInsts.empty(), "Codelet retirement arrived but no Codelets should be executing");
    //runt_codelet_t * retiredCod = pkt->getPtr<runt_codelet_t>(); // this will now be the codelet as well as a CU number (unsigned)
    //scm::instruction_state_pair * inst_pair = executingInsts.front(); // for queue version
    retire_data_t * retireData = pkt->getPtr<retire_data_t>();
    // modify state of the correct instruction
    auto cu_map_ptr = executingInsts[retireData->cuId];
    DPRINTF(SU, "Retiring Codelet %s with unique id 0x%lx\n", retireData->toRet.name, retireData->toRet.unid);
    panic_if(cu_map_ptr->empty(), "Attempting to retire codelet %s but CU %d list is empty", retireData->toRet.name, retireData->cuId);
    //scm::instruction_state_pair * inst_pair = (*cu_map_ptr)[retireData->toRet.fire];
    scm::instruction_state_pair * inst_pair = (*cu_map_ptr)[retireData->toRet.unid];
    panic_if(!inst_pair, "Instruction pair fetched from executingInsts is nullptr");
    inst_pair->second = scm::EXECUTION_DONE;
    scm::decoded_instruction_t * inst = inst_pair->first;
    /*
    void * dest, * src1, * src2;
    // get list based on fire function
    std::list<scm::instruction_state_pair *> listByFire = executingInsts[retiredCod->fire];
    std::list<scm::instruction_state_pair *>::iterator itr;
    // iterate through list of instructions with same fire function to find the matching one (same registers)
    for (itr = listByFire.begin(); itr != listByFire.end(); itr++) {
        scm::instruction_state_pair * inst_pair = *itr; 
        scm::decoded_instruction_t * inst = inst_pair->first;
        for (uint32_t op_num = 1; op_num <= MAX_NUM_OPERANDS; op_num++) {
            std::string & opStr = inst->getOpStr(op_num);
            DPRINTF(SUSCM, "Reading operand: %s\n", opStr.data());
            scm::operand_t & op = inst->getOp(op_num);
            if (scm::instructions::isRegister(opStr)) {
                // REGISTER REGISTER ADD CASE
                op.value.reg.reg_ptr = regFile->getRegisterByName(op.value.reg.reg_size, op.value.reg.reg_number);
                if (op_num == 1) {
                    dest = (void *) op.value.reg.reg_ptr;
                } else if (op_num == 2) {
                    src1 = (void *) op.value.reg.reg_ptr;
                } else if (op_num == 3) {
                    src2 = (void *) op.value.reg.reg_ptr;
                }
            } 
        }
        if (retiredCod->dest == dest && retiredCod->src1 == src1 && retiredCod->src2 == src2) {
            break;
        }
    }
    panic_if(itr == listByFire.end(), "SU received retirement for instruction it cannot find");
     */

    // Correct instruction found, set to done and remove from the list
    //(*itr)->second = scm::EXECUTION_DONE;
    //scm::decoded_instruction_t * inst = (*itr)->first;
    //listByFire.erase(itr);
    std::string codName = inst->getInstruction();
    runt_codelet_t retiredCod = retireData->toRet;
    DPRINTF(SU, "SU received retirement for codelet with %s %p %p %p\n", retiredCod.name, retiredCod.dest, retiredCod.src1, retiredCod.src2);
    DPRINTF(SU, "decoded instruction for retirement: %s\n", codName.data()); //%p %p %p\n", codName.data(), dest, src1, src2);
    cu_map_ptr->erase(retireData->toRet.unid); //remove from executing insts list
    // assuming this is correct and this codelet matches with this instruction for now....
    // mark instruction as finished executing -- codelet is now properly retired
    //inst_pair->second = scm::instruction_state::EXECUTION_DONE;
    //executingInsts.pop(); // for queue version
    // make packet response for sending
    pkt->makeResponse();
    return(true);

}

void
SU::accessTiming(PacketPtr pkt)
{
    // first do the access logic and check if it worked
    DPRINTF(SU, "SU being accessed for codelet retirement to serve request %s\n", pkt->print());
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
    if (!owner->handleRequest(pkt)) {
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
SU::handleRequest(PacketPtr pkt)
{
    if (respBlocked) {
        DPRINTF(SU, "handleRequest failed -- SU has outstanding request\n");
        // There is currently an outstanding request so we can't respond. Stall
        return false;
    }

    DPRINTF(SU, "Got request for addr %#x\n", pkt->getAddr());

    // This interface is now blocked waiting for the response to this packet.
    respBlocked = true;

    // Store the port for when we get the response
    assert(waitingPortId == -1);
    //waitingPortId = port_id;
    // don't need that since requests can only come from one port

    // Here we check address to see if a queue interaction is needed
    Addr reqAddr = pkt->getAddr();
    if (suRetRange.contains(reqAddr)) {
        // Local queue access
        // Schedule an event after queue access latency to actually access
        DPRINTF(SU, "Scheduling codelet retirement\n");
        schedule(new EventFunctionWrapper([this, pkt]{ accessTiming(pkt); },
                                      name() + ".accessEvent", true),
                clockEdge(sigLatency));
    } else {
        panic_if(!suRetRange.contains(reqAddr), "SU received request with incorrect address");
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


    return true;
}


void
SU::sendResponse(PacketPtr pkt)
{
    assert(respBlocked);
    DPRINTF(SU, "Sending resp for addr %#x\n", pkt->getAddr());

    int port = waitingPortId;

    // The packet is now done. We're about to put it in the port, no need for
    // this object to continue to stall.
    // We need to free the resource before sending the packet in case the CPU
    // tries to send another request immediately (e.g., in the same callchain).
    respBlocked = false;
    waitingPortId = -1;

    // Simply send reply back to the CodeletInterface
    codRespPort.sendPacket(pkt);

    // Need to send retry to response port so it knows we can accept new packets
    codRespPort.trySendRetry();
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
SU::sendRequest(void *toPush, Addr dest, bool isMemCod)
{
    if (reqBlocked) { //can't send if already being used
        return false;
    }
    // request is not blocked, so now lets give the codelet a unique id and increment it
    if (isMemCod) {
        runt_memcod_t * toSend = reinterpret_cast<runt_memcod_t *>(toPush);
        toSend->unid = codUniqueId;
        codUniqueId++; // increment unique codelet id for next send
        DPRINTF(SU, "Pushing codelet %s with unique id 0x%lx to interface with addr %#x\n", toSend->name, toSend->unid, dest);
        reqBlocked = true;
        // need to make a request object first to pass to the packet
        // let's say the SU has requestor ID 55..... if not, make it invalid somehow
        Request::Flags reqFlags(Request::UNCACHEABLE | Request::PHYSICAL);
        const RequestPtr codReq = std::shared_ptr<Request>(new Request(dest, sizeof(runt_memcod_t), reqFlags, 55));
        PacketPtr codPkt = Packet::createWrite(codReq);
        codPkt->dataStatic<runt_memcod_t>(toSend);
        codReqPort.sendPacket(codPkt);
    } else {
        runt_codelet_t * toSend = reinterpret_cast<runt_codelet_t *>(toPush);
        toSend->unid = codUniqueId;
        codUniqueId++; // increment unique codelet id for next send
        DPRINTF(SU, "Pushing codelet %s with unique id 0x%lx to interface with addr %#x\n", toSend->name, toSend->unid, dest);
        reqBlocked = true;
        // need to make a request object first to pass to the packet
        // let's say the SU has requestor ID 55..... if not, make it invalid somehow
        Request::Flags reqFlags(Request::UNCACHEABLE | Request::PHYSICAL);
        const RequestPtr codReq = std::shared_ptr<Request>(new Request(dest, sizeof(runt_codelet_t), reqFlags, 55));
        PacketPtr codPkt = Packet::createWrite(codReq);
        codPkt->dataStatic<runt_codelet_t>(toSend);
        codReqPort.sendPacket(codPkt);
    }
    return(true);
}

bool
SU::pushFromFD(scm::instruction_state_pair *inst_pair)
{
    // this is just here so we don't do all the work
    // of packing the runtime if we can't send yet anyways
    // can be removed when we switch codports to qports
    if (reqBlocked) return(false);
    // take instruction state pair and turn into a runtime codelet
    scm::decoded_instruction_t * inst = inst_pair->first;
    // first, to simplify we assume no immediates
    // second, this is only called from the fetch decode for execute instructions,
    // i.e. codelets, so we can assume ->instruction is the codelet name
    //char * codName = (char *) inst->getInstruction().data();
    std::string codName = inst->getInstruction();
    scm::codelet * scmCod = inst->getExecCodelet(); 
    DPRINTF(SUSCM, "Try to push codelet %s from fetch decode call\n", codName);
    void * dest = nullptr;
    void * src1 = nullptr;
    void * src2 = nullptr;
    // code below adapted from decoded_instruction_t::decodeOperands
    for (uint32_t op_num = 1; op_num <= MAX_NUM_OPERANDS; op_num++) {
        std::string & opStr = inst->getOpStr(op_num);
        //DPRINTF(SUSCM, "Reading operand: %s\n", opStr.data());
        scm::operand_t & op = inst->getOp(op_num);
        if (scm::instructions::isRegister(opStr)) {
            // REGISTER REGISTER ADD CASE
            //op.value.reg.reg_ptr = regFile->getRegisterByName(op.value.reg.reg_size, op.value.reg.reg_number);
            if (op_num == 1) {
                dest = (void *) op.value.reg.reg_ptr;
                // for some reason, gem5 crashes trying to print the reg_ptr as %p but not dest......
            } else if (op_num == 2) {
                src1 = (void *) op.value.reg.reg_ptr;
            } else if (op_num == 3) {
                src2 = (void *) op.value.reg.reg_ptr;
            }
        } else if (op.type == scm::operand_t::IMMEDIATE_VAL) {
            if (op_num == 2) {
                DPRINTF(SUSCM, "Op 2 is immediate with value 0x%lx\n", op.value.immediate);
                src1 = (void *) op.value.immediate;
            } else if (op_num == 3) {
                DPRINTF(SUSCM, "Op 3 is immediate with value 0x%lx\n", op.value.immediate);
                src2 = (void *) op.value.immediate;
            }
        }
    }
    bool request_resp;
    if (isMemcod(codName)) {
        runt_memcod_t tmp;
        tmp.fire = (fire_t) scmCod->getFireFunc();
        tmp.rng_res = (fire_t) scmCod->getResRng();
        tmp.dest = dest;
        tmp.src1 = src1;
        tmp.src2 = src2;
        strcpy(tmp.name, codName.data());
        runt_memcod_t * toPush = (runt_memcod_t *) malloc(sizeof(runt_memcod_t));
        memcpy(toPush, &tmp, sizeof(runt_memcod_t));
        // send to CodeletInterface; set the addr to the correct one based on the CU we're scheduling to
        //Addr interface_addr(0x90000000 + (cuToSchedule * (sizeof(runt_codelet_t)+sizeof(unsigned))));
        // there will be a sequential mcu arbiter that handles memory range resolution so this should
        // be able to be the space AFTER all the codelet interfaces (including the ones on each mcuthread)
        Addr mcu_interface_addr(0x90000000 + ((numCus + numMcus) * (sizeof(runt_codelet_t)+sizeof(unsigned))));
        // here need to call a function sendMemcodRequest that
        // accepts runt_memcod instead of runt_codelet
        if(sendRequest(toPush, mcu_interface_addr, true)) {
            DPRINTF(SUSCM, "Runtime codelet sent: %p\t%p\t%p\t%p\t%s\n", (void *)tmp.fire, tmp.dest, tmp.src1, tmp.src2, tmp.name);
            // add instruction that was sent to the list of instructions in execution
            auto cu_map_ptr = executingInsts[cuToSchedule];
            (* cu_map_ptr)[toPush->unid] = inst_pair;
            DPRINTF(SULoader, "Adding inst_pair %p mapped to %lx for CU %d\n", (*cu_map_ptr)[tmp.unid], (unsigned long)tmp.unid, cuToSchedule);
            cuToSchedule = (cuToSchedule + 1) % (numCus + numMcus); // enforces round robin
            return(true);
        } else {
            return(false);
        }
    } else {
        runt_codelet_t tmp;
        tmp.fire = (fire_t) scmCod->getFireFunc();
        tmp.dest = dest;
        tmp.src1 = src1;
        tmp.src2 = src2;
        strcpy(tmp.name, codName.data());
        // we do not set the codelet unique id yet because we don't want to cycle through them
        // before we actually schedule them, so we will do it in sendRequest
        // TODO: add a way to make sure this gets freed at some point
        runt_codelet_t * toPush = (runt_codelet_t *) malloc(sizeof(runt_codelet_t));
        memcpy(toPush, &tmp, sizeof(runt_codelet_t));
        // send to CodeletInterface; set the addr to the correct one based on the CU we're scheduling to
        Addr interface_addr(0x90000000 + (cuToSchedule * (sizeof(runt_codelet_t)+sizeof(unsigned))));
        if(sendRequest(toPush, interface_addr, false)) {
            DPRINTF(SUSCM, "Runtime codelet sent: %p\t%p\t%p\t%p\t%s\n", (void *)tmp.fire, tmp.dest, tmp.src1, tmp.src2, tmp.name);
            // add instruction that was sent to the list of instructions in execution
            auto cu_map_ptr = executingInsts[cuToSchedule];
            (* cu_map_ptr)[toPush->unid] = inst_pair;
            DPRINTF(SULoader, "Adding inst_pair %p mapped to %lx for CU %d\n", (*cu_map_ptr)[tmp.unid], (unsigned long)tmp.unid, cuToSchedule);
            cuToSchedule = (cuToSchedule + 1) % (numCus + numMcus); // enforces round robin
            return(true);
        } else {
            return(false);
        }
    }

}

bool
SU::commitFromFD()
{
    if (numMcus > 0) {
        runt_memcod_t * toPush = (runt_memcod_t *) malloc(sizeof(runt_memcod_t));
        toPush->fire = finalCod.fire;
        toPush->rng_res = finalCod.fire;
        Addr interface_addr(0x90000000 + (numCus+numMcus-1) * (sizeof(runt_codelet_t)+sizeof(unsigned)));
        if(sendRequest(toPush, interface_addr, false)) {
            if (numMcus > 1) {
                // numCus acts here like a number of active CUs
                numMcus--; //subtract num CUs so next tick the next interface will be sent to
            }
            // only return true when ALL CUs are turned off
        }     
        return(false);
    }
    if (numCus < 1) {
        return(true);
    } else {
        // TODO: add a way to make sure this gets freed at some point
        runt_codelet_t * toPush = (runt_codelet_t *) malloc(sizeof(runt_codelet_t));
        // since commit has been called, we send out the finalCod to turn off the CU runtime
        memcpy(toPush, &finalCod, sizeof(runt_codelet_t));
        DPRINTF(SUSCM, "Sending final codelet: %p\t%s\n", (void *)toPush->fire, toPush->name);
        // send to CodeletInterface
        // need to manage this better, with the SU having a count of CUs 
        // and managing different instruction lists for them
        Addr interface_addr(0x90000000 + (numCus-1) * (sizeof(runt_codelet_t)+sizeof(unsigned)));
        // below needs to be changed to account for MCU threads
        if(sendRequest(toPush, interface_addr, false)) {
            if (numCus > 1) {
                // numCus acts here like a number of active CUs
                numCus--; //subtract num CUs so next tick the next interface will be sent to
                return(false);
            }
            // only return true when ALL CUs are turned off
            return(true);
        } else {
            return(false);
        }
    }
}

bool
SU::fetchOperandsFromMem(scm::instruction_state_pair *inst_pair)
{
    stallingInst = inst_pair;
    scm::decoded_instruction_t * inst = inst_pair->first;
    assert(inst_pair->second == scm::instruction_state::STALL); //should be stalling while we fetch data for it
    long unsigned src1_addr = (long unsigned) inst->getOp2().value.reg.reg_ptr;
    // ---------------- Packet building for src1 --------------------------------------------------------------    
    // need to make a request object first to pass to the packet
    // let's say the SU has requestor ID 55..... if not, make it invalid somehow
    Request::Flags reqFlags(Request::PHYSICAL); //should be able to keep PHYSICAL flag since register file is mapped
    const RequestPtr src1_req = std::shared_ptr<Request>(new Request(Addr(src1_addr), sizeof(uint64_t), reqFlags, reqId));
    PacketPtr src1_pkt = Packet::createRead(src1_req);
    src1_pkt->dataStatic<uint64_t>(new uint64_t);
    DPRINTF(SUMem, "Fetching data from addr %lx for SCM instruction\n", src1_addr);
    //memPort.sendTimingReq(src1_pkt);
    memPort.schedTimingReq(src1_pkt, curTick() + 1);
    // not sure if we will have to check if the timing request succeeds...
    // maybe will have to keep an eye on consistency now that it is queued
    // ---------------- Packet building for src2 --------------------------------------------------------------    
    // src2 is immediate so we don't have to fetch it
    if (inst->getOp3().type != scm::operand_t::IMMEDIATE_VAL) {
        long unsigned src2_addr = (long unsigned) inst->getOp3().value.reg.reg_ptr;
        Request::Flags reqFlags(Request::PHYSICAL); //should be able to keep PHYSICAL flag since register file is mapped
        const RequestPtr src2_req = std::shared_ptr<Request>(new Request(Addr(src2_addr), sizeof(uint64_t), reqFlags, reqId));
        PacketPtr src2_pkt = Packet::createRead(src2_req);
        src2_pkt->dataStatic<uint64_t>(new uint64_t);
        DPRINTF(SUMem, "Fetching data from addr %lx for SCM instruction\n", src2_addr);
        //memPort.sendTimingReq(src2_pkt);
        memPort.schedTimingReq(src2_pkt, curTick() + 1);
    }
    return(true);
}

bool
SU::writebackOpToMem(uint64_t * result)
{
    if (regCopyState != FETCH_COMPLETE) {
        return(false);
    } else {
        regCopyState = WRITING;
        scm::decoded_instruction_t * inst = stallingInst->first;
        assert(stallingInst->second == scm::instruction_state::STALL); //should be stalling while we fetch data for it
        long unsigned dest_addr = (long unsigned) inst->getOp1().value.reg.reg_ptr;
        Request::Flags reqFlags(Request::PHYSICAL); //should be able to keep PHYSICAL flag since register file is mapped
        const RequestPtr dest_req = std::shared_ptr<Request>(new Request(Addr(dest_addr), sizeof(uint64_t), reqFlags, reqId));
        PacketPtr dest_pkt = Packet::createWrite(dest_req);
        dest_pkt->dataStatic<uint64_t>(result);
        // TODO: assess if there is an issue here with passing reference of parameter; probably this function should take a pointer
        DPRINTF(SUMem, "Writing back data %lx to addr %lx for SCM instruction\n", *result, dest_addr);
        //memPort.sendTimingReq(dest_pkt);
        memPort.schedTimingReq(dest_pkt, curTick() + 1);
        return(true);
    }
}

// called from fetch decode module; uses functional accesses to hide the register copy,
// only for an early implementation
void
SU::initRegMemCopy(scm::decoded_reg_t * dest, scm::decoded_reg_t * src)
{
    copyDest = dest;
    copySrc = src;
    regMemCopy = true;
    copySize = dest->reg_size_bytes;
    DPRINTF(SUSCM, "Initiating functional register copy (from %s to %s with size %d bytes)\n", src->reg_name, dest->reg_name, copySize);
    // send out all the read packets required based on the register size
    for (int i=0; i<copySize; i+=8) {
        uint64_t src_addr = (uint64_t) (src->reg_ptr + i);
        Request::Flags reqFlags(Request::PHYSICAL); //should be able to keep PHYSICAL flag since register file is mapped
        const RequestPtr src_req = std::shared_ptr<Request>(new Request(Addr(src_addr), sizeof(uint64_t), reqFlags, reqId));
        PacketPtr src_pkt = Packet::createRead(src_req);
        src_pkt->dataStatic<uint64_t>(new uint64_t);
        DPRINTF(SUSCM, "Sending out functional read packet %s\n", src_pkt->print());
        memPort.sendFunctional(src_pkt);
        // functional is instant so we should be able to immediately copy and send the dest packet
        uint64_t * ele = src_pkt->getPtr<uint64_t>();
        DPRINTF(SUSCM, "Data read functionally is 0x%lx; data is located at %p in gem5 space\n", *ele, ele);
        uint64_t dest_addr = (uint64_t) (dest->reg_ptr + i);
        Request::Flags reqFlags2(Request::PHYSICAL); //should be able to keep PHYSICAL flag since register file is mapped
        const RequestPtr dest_req = std::shared_ptr<Request>(new Request(Addr(dest_addr), sizeof(uint64_t), reqFlags2, reqId));
        PacketPtr dest_pkt = Packet::createWrite(dest_req);
        //dest_pkt->dataStatic<uint64_t>(ele);
        dest_pkt->dataStatic<uint64_t>(new uint64_t);
        memcpy(dest_pkt->getPtr<uint64_t>(), ele, sizeof(uint64_t));
        delete src_pkt->getPtr<uint64_t>();
        DPRINTF(SUSCM, "Sending out functional write packet %s with data 0x%lx\n", dest_pkt->print(), *dest_pkt->getPtr<uint64_t>());
        memPort.sendFunctional(dest_pkt);
        delete dest_pkt->getPtr<uint64_t>();
        /* functional read properly returns; so the problem is not here
        uint64_t src2_addr = (uint64_t) (src->reg_ptr + i);
        Request::Flags reqFlags3(Request::PHYSICAL); //should be able to keep PHYSICAL flag since register file is mapped
        const RequestPtr src2_req = std::shared_ptr<Request>(new Request(Addr(src_addr), sizeof(uint64_t), reqFlags3, reqId));
        PacketPtr src2_pkt = Packet::createRead(src2_req);
        src2_pkt->dataStatic<uint64_t>(new uint64_t);
        DPRINTF(SUSCM, "Sending out functional read packet %s\n", src2_pkt->print());
        memPort.sendFunctional(src2_pkt);
        DPRINTF(SUSCM, "Read packet returned value 0x%lx\n", *src2_pkt->getPtr<uint64_t>());
        delete src2_pkt->getPtr<uint64_t>();
         */
    }
    DPRINTF(SUSCM, "Functional register copy done\n");
}

 //code calling this function should be sure to free the pointer returned after use
void *
SU::fetchOp(scm::decoded_reg_t * reg)
{
    unsigned reg_size = reg->reg_size_bytes;
    //void * dest = (void *) new unsigned char[reg_size];
    unsigned char * dest = new unsigned char[reg_size];
    for (int i=0; i<reg_size; i+=8) {
        uint64_t src_addr = (uint64_t) (reg->reg_ptr + i);
        Request::Flags reqFlags(Request::PHYSICAL); //should be able to keep PHYSICAL flag since register file is mapped
        const RequestPtr src_req = std::shared_ptr<Request>(new Request(Addr(src_addr), sizeof(uint64_t), reqFlags, reqId));
        PacketPtr src_pkt = Packet::createRead(src_req);
        src_pkt->dataStatic<uint64_t>(new uint64_t);
        DPRINTF(SUSCM, "Sending functional read %s for reg. %s\n", src_pkt->print(), reg->reg_name);
        memPort.sendFunctional(src_pkt);
        DPRINTF(SUSCM, "Functional read has unsigned data 0x%lx\n", *src_pkt->getPtr<uint64_t>());
        memcpy(dest+i, src_pkt->getPtr<uint64_t>(), 8);
        delete src_pkt->getPtr<uint64_t>(); // free the pkt data we had to allocate
    }
    return((void *)dest);
}

void
SU::writeOp(scm::decoded_reg_t * reg, void * src)
{
    unsigned reg_size = reg->reg_size_bytes;
    for (int i=0; i<reg_size; i+=8) {
        uint64_t dest_addr = (uint64_t) (reg->reg_ptr + i);
        Request::Flags reqFlags(Request::PHYSICAL); //should be able to keep PHYSICAL flag since register file is mapped
        const RequestPtr dest_req = std::shared_ptr<Request>(new Request(Addr(dest_addr), sizeof(uint64_t), reqFlags, reqId));
        PacketPtr dest_pkt = Packet::createWrite(dest_req);
        dest_pkt->dataStatic<uint64_t>(new uint64_t);
        DPRINTF(SUSCM, "Sending functional write %s for reg. %s\n", dest_pkt->print(), reg->reg_name);
        memcpy(dest_pkt->getPtr<uint64_t>(), src+i, sizeof(uint64_t));
        DPRINTF(SUSCM, "Functional write has unsigned data 0x%lx\n", *(dest_pkt->getPtr<uint64_t>()));
        memPort.sendFunctional(dest_pkt);
        delete dest_pkt->getPtr<uint64_t>();
    }
        uint64_t dest_addr = (uint64_t) (reg->reg_ptr);
        Request::Flags reqFlags(Request::PHYSICAL); //should be able to keep PHYSICAL flag since register file is mapped
        const RequestPtr dest_req = std::shared_ptr<Request>(new Request(Addr(dest_addr), sizeof(uint64_t), reqFlags, reqId));
        PacketPtr dest_pkt = Packet::createRead(dest_req);
        dest_pkt->dataStatic<uint64_t>(new uint64_t);
        DPRINTF(SUSCM, "Sending read check %s for reg. %s\n", dest_pkt->print(), reg->reg_name);
        memPort.sendFunctional(dest_pkt);
        DPRINTF(SUSCM, "read check has unsigned data 0x%lx\n", *(dest_pkt->getPtr<uint64_t>()));
        delete dest_pkt->getPtr<uint64_t>();
}

// this is only called when the SU receives a response from the memPort
// which means a SCM register write is being acknowledged or
// an SCM register read is returning data, so we need to organize it
void
SU::recvTimingResp(PacketPtr pkt)
{
    if (regCopyState == EMPTY) { // no transactions complete for current instruction
        // this response should contain read data
        assert(pkt->isResponse() && pkt->hasData());
        uint64_t * dataPtr = pkt->getPtr<uint64_t>();
        memcpy(&(localRegCopies[8]), dataPtr, sizeof(uint64_t));
        DPRINTF(SU, "data packet from register file: %s\n", pkt->print());
        DPRINTF(SU, "data fetched from register file for op1 (first source): %lx\n", *((long unsigned *)&localRegCopies[8]));
        regCopyState = OP1_FETCHED;
        // if second op is immediate, then the first fetch completed means all fetches are complete
        if (stallingInst->first->getOp3().type == scm::operand_t::IMMEDIATE_VAL) {
            regCopyState = FETCH_COMPLETE;
        }
    } else if (regCopyState == OP1_FETCHED) {
        assert(pkt->isResponse() && pkt->hasData());
        uint64_t * dataPtr = pkt->getPtr<uint64_t>();
        memcpy(&(localRegCopies[16]), dataPtr, sizeof(uint64_t));
        DPRINTF(SU, "data fetched from register file for op2 (second source): %lx\n", *((long unsigned *)&localRegCopies[16]));
        regCopyState = FETCH_COMPLETE;
    } else if (regCopyState == FETCH_COMPLETE || regCopyState == WRITING) {
        // Looks like writes aren't working correctly -- or maybe the reads; values aren't persisting properly in some way
        assert(pkt->isResponse()); // double check this -- removed hasData() b/c don't know if write responses do or not
        //uint64_t * dataPtr = pkt->getPtr<uint64_t>();
        //memcpy(&(localRegCopies[16]), dataPtr, sizeof(uint64_t));
        DPRINTF(SU, "data written to register file for op3 (dest)\n");
        regCopyState = TRANS_COMPLETE;
    }
}

// -------------------------------------------------------------------------

bool
SU::MemSidePort::recvTimingResp(PacketPtr pkt)
{
    su->recvTimingResp(pkt);
    return true;
}

// Don't think snoops should ever actually occur on this port, but for now just in case
// Express snooping requests to memside port
/*
void
SU::MemSidePort::recvTimingSnoopReq(PacketPtr pkt)
{
    // Snoops shouldn't happen when bypassing caches
    assert(!cache->system->bypassCaches());

    // handle snooping requests
    cache->recvTimingSnoopReq(pkt);
}

Tick
SU::MemSidePort::recvAtomicSnoop(PacketPtr pkt)
{
    // Snoops shouldn't happen when bypassing caches
    assert(!cache->system->bypassCaches());

    return cache->recvAtomicSnoop(pkt);
}

void
SU::MemSidePort::recvFunctionalSnoop(PacketPtr pkt)
{
    // Snoops shouldn't happen when bypassing caches
    assert(!cache->system->bypassCaches());

    // functional snoop (note that in contrast to atomic we don't have
    // a specific functionalSnoop method, as they have the same
    // behaviour regardless)
    cache->functionalAccess(pkt, false);
}

void
SU::CacheReqPacketQueue::sendDeferredPacket()
{
    // sanity check
    assert(!waitingOnRetry);

    // there should never be any deferred request packets in the
    // queue, instead we resly on the cache to provide the packets
    // from the MSHR queue or write queue
    assert(deferredPacketReadyTime() == MaxTick);

    // check for request packets (requests & writebacks)
    QueueEntry* entry = cache.getNextQueueEntry();

    if (!entry) {
        // can happen if e.g. we attempt a writeback and fail, but
        // before the retry, the writeback is eliminated because
        // we snoop another cache's ReadEx.
    } else {
        // let our snoop responses go first if there are responses to
        // the same addresses
        if (checkConflictingSnoop(entry->getTarget()->pkt)) {
            return;
        }
        waitingOnRetry = entry->sendPacket(cache);
    }

    // if we succeeded and are not waiting for a retry, schedule the
    // next send considering when the next queue is ready, note that
    // snoop responses have their own packet queue and thus schedule
    // their own events
    if (!waitingOnRetry) {
        schedSendEvent(cache.nextQueueReadyTime());
    }
}
*/

SU::MemSidePort::MemSidePort(const std::string &_name,
                                    SU *_su,
                                    const std::string &_label)
    : SURequestPort(_name, _su, _reqQueue, _snoopRespQueue),
      _reqQueue(*_su, *this, _snoopRespQueue, _label),
      _snoopRespQueue(*_su, *this, true, _label), su(_su)
{
}

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
    // call on the SU's response side port...
    codRespPort.sendRangeChange();
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
    DPRINTF(SU, "Size of runtime codelet in gem5: 0x%lx\n", sizeof(runt_codelet_t));
    sendRangeChange();    
    // set reg space to correct address in runtime address spaced based on ELF section
    //regSpace = readRegSpacePtr();
    // analyze elf section data to get user codelets, they will be used 
    // by inst_mem to set scm::codelets' fire functions based on users'
    unsigned count = getCodelets();
    // Initialize scm modules
    controlStore = new scm::control_store_module(1);
    //regFile = new scm::reg_file_module((scm::register_file_t *)regSpace);
    regFile = new scm::reg_file_module((scm::register_file_t *)0x90001000);
    instructionMem = new scm::inst_mem_module(scmFileName, regFile, this);
    fetchDecode = new scm::fetch_decode_module(instructionMem, controlStore, &aliveSig, ilpMode, this, (uint64_t)(0x90001000+12288000));
    /* TODO: change SU fields that are scm modules to be pointers to the SCM modules
     * We have to do this because the register file needs to be instantiated when the
     * SU already has gotten the root point of the register file from the ELF file. This
     * means that we cannot construct the register file module at the same time, which 
     * unfortunately implies we need to wait to construct everything else as well. */
}

void
SU::startup()
{
    // here: schedule first tick
    // adding some page table checking to debug hidden register file
    /* doesn't seem to be a mapping problem... don't see anything else mapped in the region
    ThreadContext *tmp_context = system->threads[0];
    gem5::Process * tmp_proc = tmp_context->getProcessPtr();
    auto proc_map = new std::vector<std::pair<Addr, Addr>>;
    tmp_proc->pTable->getMappings(proc_map); 
    DPRINTF(SUSCM, "Printing process memory mappings\n");
    for (auto it=proc_map->cbegin(); it!=proc_map->cend(); it++) {
        DPRINTF(SUSCM, "0x%lx : 0x%lx\n", it->first, it->second);
    }
    delete proc_map;
    */
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
    // only tick the FD when it hasn't reached COMMIT yet
    if (aliveSig) {
        fetchDecode->tickBehavior();
    } 
    // keep trying to send the final codelet until it succeeds to close CU runtime
    // this will keep going until ALL CUs are turned off based on numCus
    else if (commitFromFD()) {
        finalCodSent = true;
    }
    // keep ticking if fetchDecode is still active or we haven't yet closed the CU runtime
    if (aliveSig || !finalCodSent ) {
        schedule(tickEvent, clockEdge(Cycles(1)));
    }
}

SU::SU(const SUParams &params) :
    ClockedObject(params),
    ilpMode(scm::SUPERSCALAR),
    //ilpMode(scm::OOO),
    //ilpMode(scm::OOO), // OOO ILP now takes root for hidden register file
    scmFileName(params.scm_file_name.data()),
    tickEvent([this]{ tick(); }, "SU tick",
                false, Event::CPU_Tick_Pri),
    aliveSig(true),
    system(params.system),
    reqId(params.system->getRequestorId(this)),
    sigLatency(params.sig_latency),
    capacity(params.size), //should make this more accurate later, right now it doesn't matter
    suRetRange(params.su_ret_range),
    codReqPort(params.name + ".cod_side_req_port", this),
    codRespPort(params.name + ".cod_side_resp_port", this),
    memPort(params.name + ".mem_side_port", this, "MemSidePort"),
    respBlocked(false), reqBlocked(false), originalPacket(nullptr), 
    waitingPortId(-1), 
    numCus(params.num_cus),
    numMcus(params.num_mcus),
    regCopyState(EMPTY),
    //interfaceRangeList(params.interface_range_list),
    stats(this)
{
    for (int i=0; i<numCus+numMcus; i++) {
        // prepare correct number of fire : inst state pair maps
        //executingInsts.push_back(new std::map<fire_t, scm::instruction_state_pair *>);
        executingInsts.push_back(new std::map<uint64_t, scm::instruction_state_pair *>);
    }
    finalCod.fire = (fire_t)0xffffffffffffffff;
    finalCod.dest = nullptr;
    finalCod.src1 = nullptr;
    finalCod.src2 = nullptr;
    strcpy(finalCod.name, "finalCodelet");
    //localRegCopies = (unsigned char *) malloc(sizeof(uint64_t) * 3); //enough room for 3 64b registers
    localRegCopies = (unsigned char *) new uint64_t[3]; // size enough for 3 long unsigneds but unsigned char *
    // so that they are byte accessible like FD arithmetic instructions expect
    DPRINTF(SULoader, "SCM Program loaded from %s", scmFileName);
}

Port &
SU::getPort(const std::string &if_name, PortID idx)
{
    // This is the name from the Python SimObject declaration
    if (if_name == "cod_side_req_port") {
        panic_if(idx != InvalidPortID,
                 "Cod side req of SU not a vector port");
        return(codReqPort);
    } else if (if_name == "cod_side_resp_port") {
        panic_if(idx != InvalidPortID,
                 "Cod side resp of SU not a vector port");
        // We should have already created all of the ports in the constructor
        return(codRespPort);
    } else if (if_name == "mem_side_port") {
        panic_if(idx != InvalidPortID,
                 "Mem side port of SU not a vector port");
        return(memPort);
    } else {
        // pass it along to our super class
        return(ClockedObject::getPort(if_name, idx));
    }
}

} // namespace gem5
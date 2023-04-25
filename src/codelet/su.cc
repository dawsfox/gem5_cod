
#include "codelet/su.hh"
#include "debug/SU.hh"
#include "debug/SULoader.hh"
#include "sim/system.hh"
#include "sim/process.hh"
#include "base/loader/elf_object.hh"
#include "gelf.h"
#include "codelet/codelet.hh"
#include <string.h>

namespace gem5
{

#define SYNCSLOT_SIZE 32 //just a placeholder value...

void getCodelets(System * system)
{
    ThreadContext *tmp_context = system->threads[0];
    loader::ObjectFile *tmp_obj = tmp_context->getProcessPtr()->objFile;
    loader::ElfObject *elfObject = dynamic_cast<loader::ElfObject *>(tmp_obj);
    DPRINTF(SULoader, "elfObject has address %p\n", elfObject);
    Elf * real_elf = elfObject->getElf();
    DPRINTF(SULoader, "real elf has address %p\n", real_elf);
    // we added getElf so we can go grab the Codelet section
    size_t string_index;
    // get index of string table so we can find section names
    elf_getshdrstrndx(real_elf, &string_index);
    DPRINTF(SULoader, "string table is at section %u\n", string_index);
    Elf_Scn *section = elf_getscn(real_elf, 1);
    DPRINTF(SULoader, "first section is at %p\n", section);
    bool program_found = false;
    for (int sec_idx = 1; section; section = elf_getscn(real_elf, ++sec_idx)) {
        GElf_Shdr shdr;
        gelf_getshdr(section, &shdr);
        DPRINTF(SULoader, "Got section header!\n");
        // sh_name is the index in the string table where name is located, so check it in string table
        // Return pointer to string at OFFSET in section INDEX.
        char * section_name = elf_strptr(real_elf, string_index, shdr.sh_name);
        DPRINTF(SULoader, "checking section %s\n", section_name);
        // if section name is .codelet_program
        if (!strcmp(section_name, ".codelet_program")) {
            DPRINTF(SULoader, "section %s FOUND\n", section_name);
            program_found = true;
            break;
        }    
    }
    if (program_found) {
        // actually copy over the codelets now
        //extern Elf_Data *elf_getdata (Elf_Scn *__scn, Elf_Data *__data);
        Elf_Data *prog_data = elf_getdata(section, NULL);
        DPRINTF(SULoader, "Codelet program data located at %p\n", prog_data);
        codelet_t * codelet_program = (codelet_t *) prog_data->d_buf;
        size_t codelet_count = prog_data->d_size / sizeof(codelet_t);
        DPRINTF(SULoader, "Codelet program located at %p with %u codelets\n", codelet_program, codelet_count);
    }

}

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

    schedule(new EventFunctionWrapper([this, params]{ getCodelets(params.system); },
                                    name() + ".loadCodeletsEvent", true),
                    clockEdge(sigLatency));
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
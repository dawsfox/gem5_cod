#ifndef __CODELET_SU_HH__
#define __CODELET_SU_HH__

#include "base/statistics.hh"
#include "mem/port.hh"
#include "params/SU.hh"
#include "sim/clocked_object.hh"
#include "codelet/codelet.hh"
// mixed naming conventions :/
#include "codelet/SCMUlate/include/modules/fetch_decode.hpp"
#include "codelet/SCMUlate/include/modules/control_store.hpp"
#include "codelet/SCMUlate/include/modules/instruction_mem.hpp"
#include "codelet/SCMUlate/include/modules/register.hpp"
//#include <queue>
#include <list>

namespace gem5
{

class SU : public ClockedObject
{
  private:
    class CodSideReqPort : public RequestPort
    {
      private:
        //int id; // This will be a vector port later when there are more CUs?
        SU *owner;
        PacketPtr blockedPacket;

      public:
        CodSideReqPort(const std::string& name, SU *owner) :
            RequestPort(name, owner), owner(owner), blockedPacket(nullptr)
        { }

        void sendPacket(PacketPtr pkt);

      protected:
        bool recvTimingResp(PacketPtr pkt) override;

        /**
         * Called by the response port if sendTimingReq was called on this
         * request port (causing recvTimingReq to be called on the response
         * port) and was unsuccesful.
         */
        void recvReqRetry() override;

        /**
         * Called to receive an address range change from the peer response
         * port. The default implementation ignores the change and does
         * nothing. Override this function in a derived class if the owner
         * needs to be aware of the address ranges, e.g. in an
         * interconnect component like a bus.
         */
        void recvRangeChange() override;
        // might not need this function; only messages sent through request
        // port should be originated in the SU

    }; // class CodSideReqPort

    class CodSideRespPort : public ResponsePort
    {
      private:
        int id; //vector port needs id
        SU *owner;
        bool needRetry;
        PacketPtr blockedPacket;
        AddrRange suRange;

      public:
        CodSideRespPort(const std::string& name, int id, SU *owner) :
            ResponsePort(name, owner), id(id), owner(owner), needRetry(false),
            blockedPacket(nullptr), suRange((id==0) ? owner->suSigRange : owner->suRetRange)
        { }

        void sendPacket(PacketPtr pkt);

        /**
         * Get a list of the non-overlapping address ranges the owner is
         * responsible for. All response ports must override this function
         * and return a populated list with at least one item.
         *
         * @return a list of ranges responded to
         */
        AddrRangeList getAddrRanges() const override; 

        void trySendRetry();

      protected:
        Tick recvAtomic(PacketPtr pkt) override
        { panic("recvAtomic unimpl."); }

        void recvFunctional(PacketPtr pkt) override;

        /**
         * Receive a timing request from the request port.
         *
         * @param the packet that the requestor sent
         * @return whether this object can consume to packet. If false, we
         *         will call sendRetry() when we can try to receive this
         *         request again.
         */
        bool recvTimingReq(PacketPtr pkt) override;

        /**
         * Called by the request port if sendTimingResp was called on this
         * response port (causing recvTimingResp to be called on the request
         * port) and was unsuccessful.
         */
        void recvRespRetry() override;
    }; // class CodSideRespPort

    /**
     * Handle the request from the CPU side. Called from the CPU port
     * on a timing request.
     *
     * @param requesting packet
     * @param id of the port to send the response
     * @return true if we can handle the request this cycle, false if the
     *         requestor needs to retry later
     */
    bool handleRequest(PacketPtr pkt, int port_id);

    /**
     * Handle the respone from the memory side. Called from the cod resp port
     * on a timing response.
     *
     * @param responding packet
     * @return true if we can handle the response this cycle, false if the
     *         responder needs to retry later
     */
    bool handleResponse(PacketPtr pkt);

    /**
     * Send the packet to the CPU side.
     * This function assumes the pkt is already a response packet and forwards
     * it to the correct port. This function also unblocks this object and
     * cleans up the whole request.
     *
     * @param the packet to send to the cpu side
     */
    void sendResponse(PacketPtr pkt);

    /**
     * Handle a packet functionally. Update the data on a write and get the
     * data on a read. Called from CPU port on a recv functional.
     *
     * @param packet to functionally handle
     */
    void handleFunctional(PacketPtr pkt);

    /**
     * Access the cache for a timing access. This is called after the cache
     * access latency has already elapsed.
     */
    void accessTiming(PacketPtr pkt);

    /**
     * This is where we actually update / read from the cache. This function
     * is executed on both timing and functional accesses.
     *
     * @return true if a hit, false otherwise
     */
    bool accessFunctional(PacketPtr pkt);

    // used to push Codelets out to CUs
    bool sendRequest(codelet_t *toPush, Addr dest);

    /**
     * Tell the CPU side to ask for our memory ranges.
     */
    void sendRangeChange() const;

    // Loads the Codelets from the user space program
    // they are located in a specifically namd elf section
    // return number of codelets retrieved
    unsigned getCodelets();
    
    // reads root address of register space for CU runtime from elf section
    // and sets regSpace
    unsigned char * readRegSpacePtr();

    /* SCM modules go here; they will handle behavior based on the SCM
     * program. Fetch decode module is used to schedule instructions 
     * based on the instruction level parallelism instantiation. The
     * instruction level parallelism instantiation is chosen based
     * on the ilpMode parameter. Function for pushing Codelets to CUs will
     * be replaced with one that pushes through a request packet, and
     * Codelets finishing will be triggered when a codelet retirement
     * request comes from the CodeletInterface */

    //scm::reg_file_module regFile; // needed to created instructionMem; will take root pointer
    //scm::control_store_module controlStore;
    //scm::inst_mem_module instructionMem; // needed for fetchDecode. Will take regFile and scmFileName
    scm::reg_file_module * regFile; // needed to created instructionMem; will take root pointer
    scm::control_store_module * controlStore;
    scm::inst_mem_module * instructionMem; // needed for fetchDecode. Will take regFile and scmFileName

    /* Try to pass controlStore as nullptr to fetchDecode if possible, because we will replace the
     * mechanisms for execution that it provides. Alternatively, could alter the controlStore to 
     * provide the functionality we want. That would probably be better coding practice */
    // ilpMode can be: SEQUENTIAL, SUPERSCALAR, OOO
    scm::ILP_MODES ilpMode;
    //scm::fetch_decode_module fetchDecode;
    scm::fetch_decode_module * fetchDecode;

    //std::string scmFileName;
    const char * scmFileName;

    // event used to perform tasks and advance cycles
    EventFunctionWrapper tickEvent;

    codelet_t finalCod = {(fire_t)0xffffffffffffffff, 0,0,0,0, "finalCodelet"}; //final codelet definition

    bool aliveSig;

    // Params
    System * system;
    /// Latency representing dependency signaling overhead
    const Cycles sigLatency;
    /// Number of slots in Codelet queue
    const unsigned capacity;
    // range for dep. signalling
    AddrRange suSigRange;
    // range for codelet retirement
    AddrRange suRetRange;
    // codReqPort used for pushing Codelets to CU
    CodSideReqPort codReqPort;
    // codRespPorts used for receiving Codelet retirements and dependency signaling from CU
    std::vector<CodSideRespPort> codRespPorts;

    // port flow control -- may be changed later for queued ports
    bool blocked;
    bool reqBlocked;

    /// Packet that we are currently handling. Used for upgrading to larger
    /// cache line sizes
    PacketPtr originalPacket;

    /// The port to send the response when we recieve it back
    int waitingPortId;

    /// For tracking the miss latency
    Tick missTime;

    std::list<scm::decoded_instruction_t> executingInsts;

    // Size should probably be made a SU param later
    // honestly will probably be able to get rid of this later
    runt_codelet_t codSpace[32];
    std::map<std::string, fire_t> codMapping;

    unsigned char * regSpace = nullptr;

    /// SU statistics
  protected:
    struct SUStats : public statistics::Group
    {
        SUStats(statistics::Group *parent);
        statistics::Scalar cods;
        statistics::Scalar sigs;
        statistics::Histogram codLatency;
    } stats;

  public:

    // function for fetch decode unit to call when it needs to push a codelet
    bool pushFromFD(scm::instruction_state_pair *inst_pair);
    fire_t getCodeletFire(std::string codName);

    // trying this; SU is not sending range change on startup....
    void init() override;
    void startup() override;
    void tick();

    /** constructor
     */
    SU(const SUParams &params);

    /**
     * Get a port with a given name and index. This is used at
     * binding time and returns a reference to a protocol-agnostic
     * port.
     *
     * @param if_name Port name
     * @param idx Index in the case of a VectorPort
     *
     * @return A reference to the given port
     */
    Port &getPort(const std::string &if_name,
                  PortID idx=InvalidPortID) override;

};


} // namespace gem5

#endif // __CODELET_SU_HH__
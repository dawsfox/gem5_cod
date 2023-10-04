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
#include "mem/qport.hh"
#include <set>

namespace gem5
{

class SU : public ClockedObject
{
  public:
    typedef enum {
      EMPTY = 0,
      OP1_FETCHED = 1,
      OP2_FETCHED = 2,
      FETCH_COMPLETE = 3,
      WRITING = 4,
      OP3_WRITTEN = 5,
      TRANS_COMPLETE = 7
    } localRegState;
    
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
        //int id; //vector port needs id
        SU *owner;
        bool needRetry;
        PacketPtr blockedPacket;
        AddrRange suRange;

      public:
        CodSideRespPort(const std::string& name, SU *owner) :
            ResponsePort(name, owner), owner(owner), needRetry(false),
            blockedPacket(nullptr), suRange(owner->suRetRange)
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

    class SURequestPort : public QueuedRequestPort
    {

      public:

        /**
         * Schedule a send of a request packet (from the MSHR). Note
         * that we could already have a retry outstanding.
         */
        void schedSendEvent(Tick time)
        {
            //DPRINTF(CachePort, "Scheduling send event at %llu\n", time);
            reqQueue.schedSendEvent(time);
        }

      protected:

        SURequestPort(const std::string &_name, SU *_su,
                        ReqPacketQueue &_reqQueue,
                        SnoopRespPacketQueue &_snoopRespQueue) :
            QueuedRequestPort(_name, _su, _reqQueue, _snoopRespQueue)
        { }

        /**
         * Memory-side port always snoops.
         * (not on my watch it doesn't)
         * @return always false
         */
        virtual bool isSnooping() const { return false; }
    };

    /**
     * Override the default behaviour of sendDeferredPacket to enable
     * the memory-side cache port to also send requests based on the
     * current MSHR status. This queue has a pointer to our specific
     * cache implementation and is used by the MemSidePort.
     */
    class SUReqPacketQueue : public ReqPacketQueue
    {

      protected:

        SU &su;
        SnoopRespPacketQueue &snoopRespQueue;

      public:

        SUReqPacketQueue(SU &su, RequestPort &port,
                            SnoopRespPacketQueue &snoop_resp_queue,
                            const std::string &label) :
            ReqPacketQueue(su, port, label), su(su),
            snoopRespQueue(snoop_resp_queue) { }

        /**
         * Override the normal sendDeferredPacket and do not only
         * consider the transmit list (used for responses), but also
         * requests.
         */
        //virtual void sendDeferredPacket() {};

        /**
         * Check if there is a conflicting snoop response about to be
         * send out, and if so simply stall any requests, and schedule
         * a send event at the same time as the next snoop response is
         * being sent out.
         *
         * @param pkt The packet to check for conflicts against.
         */
        bool checkConflictingSnoop(const PacketPtr pkt)
        {   /*
            if (snoopRespQueue.checkConflict(pkt, cache.blkSize)) {
                DPRINTF(CachePort, "Waiting for snoop response to be "
                        "sent\n");
                Tick when = snoopRespQueue.deferredPacketReadyTime();
                schedSendEvent(when);
                return true;
            }
            */
            return false;
        }
    };


    /**
     * The memory-side port extends the base cache request port with
     * access functions for functional, atomic and timing snoops.
     */
    class MemSidePort : public SURequestPort
    {
      private:

        /** The cache-specific queue. */
        SUReqPacketQueue _reqQueue;

        SnoopRespPacketQueue _snoopRespQueue;

        // a pointer to our specific cache implementation
        SU *su;

      protected:

        virtual void recvTimingSnoopReq(PacketPtr pkt) {};

        virtual bool recvTimingResp(PacketPtr pkt);

        virtual Tick recvAtomicSnoop(PacketPtr pkt) {};

        virtual void recvFunctionalSnoop(PacketPtr pkt) {};

      public:

        MemSidePort(const std::string &_name, SU *_su,
                    const std::string &_label);
    };

    /**
     * Handle the request from the CPU side. Called from the CPU port
     * on a timing request.
     *
     * @param requesting packet
     * @param id of the port to send the response
     * @return true if we can handle the request this cycle, false if the
     *         requestor needs to retry later
     */
    bool handleRequest(PacketPtr pkt);

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
    bool sendRequest(runt_codelet_t *toPush, Addr dest);

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

    // handles the responses coming back from the (queued) memPort;
    // should organize data in a way that the SU/FD is aware when
    // all required data is available for an instruction to execute
    void recvTimingResp(PacketPtr pkt);

    /* SCM modules go here; they will handle behavior based on the SCM
     * program. Fetch decode module is used to schedule instructions 
     * based on the instruction level parallelism instantiation. The
     * instruction level parallelism instantiation is chosen based
     * on the ilpMode parameter. Function for pushing Codelets to CUs will
     * be replaced with one that pushes through a request packet, and
     * Codelets finishing will be triggered when a codelet retirement
     * request comes from the CodeletInterface */

    scm::reg_file_module * regFile; // needed to created instructionMem; will take root pointer
    scm::control_store_module * controlStore;
    scm::inst_mem_module * instructionMem; // needed for fetchDecode. Will take regFile and scmFileName

    // ilpMode can be: SEQUENTIAL, SUPERSCALAR, OOO
    scm::ILP_MODES ilpMode;
    //scm::fetch_decode_module fetchDecode;
    scm::fetch_decode_module * fetchDecode;

    //std::string scmFileName;
    const char * scmFileName;

    // event used to perform tasks and advance cycles
    EventFunctionWrapper tickEvent;

    runt_codelet_t finalCod; 

    bool aliveSig;
    bool finalCodSent = false;

    // Params
    System * system;
    // For building requests
    RequestorID reqId;
    /// Latency representing dependency signaling overhead
    const Cycles sigLatency;
    /// Number of slots in Codelet queue
    const unsigned capacity;
    // range for dep. signalling
    //AddrRange suSigRange;
    // range for codelet retirement
    AddrRange suRetRange;
    // codReqPort used for pushing Codelets to CU
    CodSideReqPort codReqPort;
    // codRespPorts used for receiving Codelet retirements and dependency signaling from CU
    //std::vector<CodSideRespPort> codRespPorts;
    CodSideRespPort codRespPort;

    MemSidePort memPort;

    // port flow control -- may be changed later for queued ports
    bool respBlocked;
    bool reqBlocked;

    /// Packet that we are currently handling. Used for upgrading to larger
    /// cache line sizes
    PacketPtr originalPacket;

    /// The port to send the response when we recieve it back
    int waitingPortId;

    // Passed from user code, indicates the beginning of SCM memory space
    uint64_t scmBasePtr;
    // Set of instructions that are executing on CUs
    // Used on retirement to find the instruction to change state of 
    // Queue is fine for scm::SEQUENTIAL but won't work for superscalar or ooo
    // So let's use a map of fire function : instruction_state_pair *
    //std::map<fire_t, std::list<scm::instruction_state_pair *>> executingInsts; //replaced by below

    // Mapping of fire to instruction per CU for instructions that are either executing or already deployed
    // to the Codelet Interface. Vector of pointers to fire:inst_pair maps
    // May need to be changed later; problems expected if multiple codelets with same fire function
    // are deployed to the same CU
    //std::vector<std::map<fire_t, scm::instruction_state_pair *> *> executingInsts;
    std::vector<std::map<uint64_t, scm::instruction_state_pair *> *> executingInsts;

    unsigned numCus; // number of CUs this SU is managing

    unsigned numMcus; // number of MCU threads this SU is managing

    unsigned cuToSchedule = 0; // used for round robin scheduling to CUs

    std::map<std::string, user_codelet_t> codMapping;

    unsigned char * regSpace = nullptr;

    // Holds local copies of registers; used to organize mem reads of scm registers
    // organized like: dest, src1, src2
    unsigned char * localRegCopies;
    // Keeps track of which registers' contents have been received from memory
    localRegState regCopyState; 
    // Pointer to scm instruction state pair that the reg copies are for
    scm::instruction_state_pair * stallingInst;

    // Used for register copying (for renaming in OoO)
    scm::decoded_reg_t * copyDest;
    scm::decoded_reg_t * copySrc;
    unsigned copySize;
    unsigned copyReceived;
    bool regMemCopy = false;

    uint64_t codUniqueId = 0;


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
    // function for fetch decode unit to call when commit instruction is reached
    bool commitFromFD();
    // function for fetch decode unit to call when reading register values to operate on them locally
    bool fetchOperandsFromMem(scm::instruction_state_pair *inst_pair);
    // function for fetch decode unit to call when writing back to a register from a local operation
    // operates based on the stallingInst
    bool writebackOpToMem(uint64_t * result);

    // Accessor methods for FD unit to access the register data that was fetched and its state
    long unsigned getLocalDest() { return(*(&localRegCopies[0])); }
    long unsigned getLocalSrc1() { return(*(&localRegCopies[8])); }
    long unsigned getLocalSrc2() { return(*(&localRegCopies[16])); }
    unsigned char * getLocalDestPtr() { return(&localRegCopies[0]); }
    unsigned char * getLocalSrc1Ptr() { return(&localRegCopies[8]); }
    unsigned char * getLocalSrc2Ptr() { return(&localRegCopies[16]); }
    localRegState getCopyState() { return(regCopyState); }
    scm::instruction_state_pair * getStallingInst() { return(stallingInst); }
    void clearStallingInst() { stallingInst = nullptr; regCopyState = EMPTY;}
    void initRegMemCopy(scm::decoded_reg_t * dest, scm::decoded_reg_t * src);
    
    uint64_t getScmBasePtr() { return(scmBasePtr); }

    fire_t getCodeletFire(std::string codName);
    uint16_t getCodeletIo(std::string codName);
    void * fetchOp(scm::decoded_reg_t * reg);
    void writeOp(scm::decoded_reg_t * reg, void * src);

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
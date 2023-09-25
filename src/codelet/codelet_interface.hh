#ifndef __CODELET_CODELET_INTERFACE_HH__
#define __CODELET_CODELET_INTERFACE_HH__

#include "base/statistics.hh"
#include "mem/port.hh"
#include "mem/qport.hh"
#include "params/CodeletInterface.hh"
#include "debug/CodeletInterface.hh"
#include "sim/clocked_object.hh"
#include "codelet/codelet.hh"
#include <queue>

namespace gem5
{
class CodeletInterface : public ClockedObject
{
  private:

    /*
    class CPUSidePort : public ResponsePort
    {
      private:
        /// Since this is a vector port, need to know what number this one is
        int id;

        CodeletInterface *owner;

        bool needRetry;
        PacketPtr blockedPacket;

      public:
        CPUSidePort(const std::string& name, int id, CodeletInterface *owner) :
            ResponsePort(name, owner), id(id), owner(owner), needRetry(false),
            blockedPacket(nullptr)
        { }

        void sendPacket(PacketPtr pkt);

        AddrRangeList getAddrRanges() const override;

        void trySendRetry();

      protected:
        Tick recvAtomic(PacketPtr pkt) override
        { panic("recvAtomic unimpl."); }

        void recvFunctional(PacketPtr pkt) override;

        bool recvTimingReq(PacketPtr pkt) override;

        void recvRespRetry() override;
    };
     */

    class CIResponsePort : public QueuedResponsePort
    {

      public:

        /** Do not accept any new requests. */
        void setBlocked();

        /** Return to normal operation and accept new requests. */
        void clearBlocked();

        bool isBlocked() const { return blocked; }

      protected:

        CIResponsePort(const std::string &_name, CodeletInterface *_ci,
                       const std::string &_label);

        /** A normal packet queue used to store responses. */
        RespPacketQueue queue;

        bool blocked = false;

        bool mustSendRetry;

      private:

        void processSendRetry();

        EventFunctionWrapper sendRetryEvent;

    };

    /**
     * The CPU-side port extends the base cache response port with access
     * functions for functional, atomic and timing requests.
     */
    class CPUSidePort : public CIResponsePort
    {
      private:

        // a pointer to our specific cache implementation
        CodeletInterface *ci;

      protected:
        virtual bool recvTimingSnoopResp(PacketPtr pkt) override { return false;}

        virtual bool tryTiming(PacketPtr pkt) override;

        virtual bool recvTimingReq(PacketPtr pkt) override;

        //virtual Tick recvAtomic(PacketPtr pkt) override;
        Tick recvAtomic(PacketPtr pkt) override
        { panic("recvAtomic unimpl."); }

        virtual void recvFunctional(PacketPtr pkt) override;

        virtual AddrRangeList getAddrRanges() const override;

      public:

        CPUSidePort(const std::string &_name, CodeletInterface *_ci,
                    const std::string &_label);

    };

    /*
    class MemSidePort : public RequestPort
    {
      private:
        CodeletInterface *owner;
        PacketPtr blockedPacket;

      public:
        MemSidePort(const std::string& name, CodeletInterface *owner) :
            RequestPort(name, owner), owner(owner), blockedPacket(nullptr)
        { }

        void sendPacket(PacketPtr pkt);

      protected:
        bool recvTimingResp(PacketPtr pkt) override;

        void recvReqRetry() override;

        void recvRangeChange() override;

    };
     */
    class CIRequestPort : public QueuedRequestPort
    {

      public:

        /**
         * Schedule a send of a request packet (from the MSHR). Note
         * that we could already have a retry outstanding.
         */
        void schedSendEvent(Tick time)
        {
            //DPRINTF(CodeletInterface, "Mem port scheduling send event at %llu\n", time);
            reqQueue.schedSendEvent(time);
        }

      protected:

        CIRequestPort(const std::string &_name, CodeletInterface *_ci,
                        ReqPacketQueue &_reqQueue,
                        SnoopRespPacketQueue &_snoopRespQueue) :
            QueuedRequestPort(_name, _ci, _reqQueue, _snoopRespQueue)
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
    class CIReqPacketQueue : public ReqPacketQueue
    {

      protected:

        CodeletInterface &ci;
        SnoopRespPacketQueue &snoopRespQueue;

      public:

        CIReqPacketQueue(CodeletInterface &ci, RequestPort &port,
                            SnoopRespPacketQueue &snoop_resp_queue,
                            const std::string &label) :
            ReqPacketQueue(ci, port, label), ci(ci),
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
    class MemSidePort : public CIRequestPort
    {
      private:

        /** The cache-specific queue. */
        CIReqPacketQueue _reqQueue;

        SnoopRespPacketQueue _snoopRespQueue;

        // a pointer to our specific cache implementation
        CodeletInterface *ci;

      protected:

        virtual void recvTimingSnoopReq(PacketPtr pkt) {};

        virtual bool recvTimingResp(PacketPtr pkt);

        virtual Tick recvAtomicSnoop(PacketPtr pkt) {};

        virtual void recvFunctionalSnoop(PacketPtr pkt) {};

        void recvRangeChange();

      public:

        MemSidePort(const std::string &_name, CodeletInterface *_ci,
                    const std::string &_label);
    };

    class CodSideReqPort : public RequestPort
    {
      private:
        CodeletInterface *owner;
        PacketPtr blockedPacket;

      public:
        CodSideReqPort(const std::string& name, CodeletInterface *owner) :
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

    }; // class CodSideReqPort

    class CodSideRespPort : public ResponsePort
    {
      private:
        CodeletInterface *owner;

        bool needRetry;
        PacketPtr blockedPacket;

      public:
        CodSideRespPort(const std::string& name, CodeletInterface *owner) :
            ResponsePort(name, owner), owner(owner), needRetry(false),
            blockedPacket(nullptr)
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
     * Handle the respone from the memory side. Called from the memory port
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
    void sendResponse(PacketPtr pkt, int port_id);

    /**
     * Handle a packet functionally. Update the data on a write and get the
     * data on a read. Called from CPU port on a recv functional.
     *
     * @param packet to functionally handle
     */
    void handleFunctional(PacketPtr pkt, int port_id);

    /**
     * Access the cache for a timing access. This is called after the cache
     * access latency has already elapsed.
     */
    void accessTiming(PacketPtr pkt, int port_id);

    /**
     * This is where we actually update / read from the cache. This function
     * is executed on both timing and functional accesses.
     *
     * @return true if a hit, false otherwise
     */
    bool accessFunctional(PacketPtr pkt, int port_id);

    /**
     * Insert a block into the cache. If there is no room left in the cache,
     * then this function evicts a random entry t make room for the new block.
     *
     * @param packet with the data (and address) to insert into the cache
     */
    void insert(PacketPtr pkt) {}

    /**
     * Return the address ranges this interface is responsible for 
     * along the Codelet bus -- i.e. SU address space and local 
     * Codelet queue. Expect to be called from CPU
     *
     * @return the address ranges this interface is responsible for
     */
    AddrRangeList getCodAddrRanges() const;

    // same as above but for mem bus instead of codelet bus
    // combined with CodAddrRanges represents whole address space
    // accessible from CPU
    AddrRangeList getMemAddrRanges() const;

    // Local addr range i.e. the local queue space (only thing accessible)
    // by SU through the interface
    AddrRangeList getLocAddrRanges() const;

    // Used for handling responses from memory requests
    void recvMemTimingResp(PacketPtr pkt);

    /**
     * Tell the CPU side to ask for our memory ranges.
     */
    void sendRangeChange() const;

    // SU is teling us to get mem ranges again?
    void sendCodRangeChange() const;

    void init() override;

    /// Latency to read or pop from the codelet queue
    const Cycles queueLatency;

    /// General overhead of packets passing through the interface
    const Cycles genLatency;

    /// Number of slots in the Codelet queue
    const unsigned capacity;

    /// Address range of the local Codelet queue
    AddrRange queueRange;

    unsigned cuId;

    // address that should be used for codelet retirement requests sent to the SU
    Addr suRetAddr;

    /// Instantiation of the CPU-side ports, memory side port, and Codelet Side port
    // CPUSidePort should be used for normal mem requests, as well as popping Codelets from queue (mem mapped)
    CPUSidePort cpuPort;
    MemSidePort memPort;
    // codReqPorts used for Codelet Retirement and decrementing dependencies
    //std::vector<CodSideReqPort> codReqPorts;
    CodSideReqPort codReqPort;
    // codRespPort used for receiving Codelets from SU
    CodSideRespPort codRespPort;

    /// True if this cache is currently blocked waiting for a response.
    bool blocked;

    /// Packet that we are currently handling. Used for upgrading to larger
    /// cache line sizes
    PacketPtr originalPacket;

    /// The port to send the response when we recieve it back
    int waitingPortId;

    /// For tracking the miss latency
    Tick missTime;

    /// FIFO Codelet queue
    std::queue<runt_codelet_t> codQueue;

    // unsigned but functions as bool
    // gives the CPU a flag to check if there's a codelet
    unsigned codeletAvailable = 0;

    // a staging location for the CPU to be able to read multiple fields 
    // as much as it wants until the codelet is retired
    runt_codelet_t activeCodelet = {nullptr, nullptr, nullptr, nullptr, ""};

    /// CodeletInterface statistics
  protected:
    struct CodeletInterfaceStats : public statistics::Group
    {
        CodeletInterfaceStats(statistics::Group *parent);
        statistics::Scalar cods;
        statistics::Scalar sigs;
        statistics::Histogram codLatency;
    } stats;

  public:

    /** constructor
     */
    CodeletInterface(const CodeletInterfaceParams &params);

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

#endif // __CODELET_CODELET_INTERFACE_HH__
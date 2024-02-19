#ifndef __MCSQUARE_H__
#define __MCSQUARE_H__

#include <deque>
#include <string>
#include <unordered_set>
#include <utility>
#include <list>

#include "base/callback.hh"
#include "base/statistics.hh"
#include "enums/MemSched.hh"
#include "mem/qos/mem_ctrl.hh"
#include "mem/qport.hh"
#include "params/MemCtrl.hh"
#include "sim/eventq.hh"
#include "sim/sim_object.hh"
#include "params/MCSquare.hh"
#include "debug/MCSquare.hh"

namespace gem5
{

// Helpers to simplify code
bool isMCSquare(const RequestPtr req);
bool isMCSquare(const gem5::Packet* pkt);
bool isMCReq(const gem5::Packet* pkt);

namespace memory
{

class MemInterface;
class DRAMInterface;
class NVMInterface;

class MCSquare : public SimObject {
    const int ctt_max_sz;
    const Tick ctt_acc_lat;
    struct CTTableEntry {
      Addr dest;
      Addr src;
      uint64_t size;

      CTTableEntry() {}
      CTTableEntry(Addr dest, Addr src, uint64_t size) {
        this->dest = dest;
        this->src = src;
        this->size = size;
      }
    };
    std::list<CTTableEntry> m_ctt;

    const int bpq_max_sz;
    const Tick bpq_acc_lat;
    class BouncePendingQueue {
      struct BouncePendingQueueEntry {
        Addr addr;
        uint8_t data[64];
        int outstandingPkts;
        BouncePendingQueueEntry() {
          addr = 0;
          outstandingPkts = -1;
        }
        BouncePendingQueueEntry(Addr addr, uint8_t *data) {
          this->addr = addr;
          outstandingPkts = -1;
          memcpy(this->data, data, 64);
        }
      };
      std::map<Addr, BouncePendingQueueEntry> m_table;

    public:
      void insert(Addr addr, uint8_t *data) {
        m_table[addr] = BouncePendingQueueEntry(addr, data);
      }

      void remove(Addr addr) {
        m_table.erase(addr);
      }

      Addr find(Addr addr) {
        if(m_table.find(addr) != m_table.end())
          return m_table[addr].addr;
        return 0;
      }

      int getPkts(Addr addr) {
        if(m_table.find(addr) != m_table.end())
          return m_table[addr].outstandingPkts;
        return 0;
      }

      bool setPkts(Addr addr, int pkts) {
        if(m_table.find(addr) != m_table.end())
          return (m_table[addr].outstandingPkts = pkts);
        return false;
      }

      void decPkts(Addr addr, int pkts = 1) {
        if(m_table.find(addr & (uint64_t)(~63)) != m_table.end())
          if(m_table[addr & (uint64_t)(~63)].outstandingPkts > 0) {
            m_table[addr & (uint64_t)(~63)].outstandingPkts -= pkts;
          }

        if(addr % 64 != 0)
          if(m_table.find((addr + 64) & (uint64_t)(~63)) != m_table.end())
            if(m_table[(addr + 64) & (uint64_t)(~63)].outstandingPkts > 0) {
              m_table[(addr + 64) & (uint64_t)(~63)].outstandingPkts -= pkts;
            }
      }

      uint8_t *getData(Addr addr) {
        if(m_table.find(addr) == m_table.end())
          return NULL;
        return m_table[addr].data;
      }

      auto begin() {
        return m_table.begin();
      }

      auto end() {
        return m_table.end();
      }

      bool setData(Addr addr, uint8_t *data) {
        if(find(addr) != 0) {
          memcpy(m_table[addr].data, data, 64);
          return true;
        }
        return false;
      }

      size_t size() {
        return m_table.size();
      }
    };
    
    // Whether read to dest should create a duplicate write packet
    const int wb_dest_reads;
  public:
    const double ctt_free_frac;
    Addr ctt_src_entry;
    int ctt_count;

    int wbDestReads() {
      return wb_dest_reads;
    }

    enum Types {
      TYPE_NONE = 0,
      TYPE_SRC,
      TYPE_DEST,
    };

    MCSquare(const MCSquareParams &params) : SimObject(params),
      ctt_max_sz(params.ctt_size), ctt_acc_lat(params.ctt_penalty), 
      bpq_max_sz(params.bpq_size), bpq_acc_lat(params.bpq_penalty), 
      wb_dest_reads(params.wb_dest_reads), ctt_free_frac(params.ctt_free),
      stats(*this) {
        printf("Created MCSquare with: CTT (%d size, %lu ticks, %.2f%% free frac), ",
          ctt_max_sz, ctt_acc_lat, ctt_free_frac * 100);
        printf("BPQ (%d size, %lu ticks)\n", bpq_max_sz, bpq_acc_lat);
        ctt_src_entry = 0;
        ctt_count = 0;
      }

    // CTT management functions
    void insertEntry(Addr dest, Addr src, uint64_t size);
    void deleteEntry(Addr dest, uint64_t size);
    void splitEntry(PacketPtr pkt);
    Addr getAddrToFree(AddrRangeList addrList);
    // Check CTT
    Types contains(Addr addr, size_t size);
    Types contains(PacketPtr pkt);
    bool isSrc(PacketPtr pkt);
    bool isDest(PacketPtr pkt);
    size_t getCTTSize() { return m_ctt.size(); }

    // Bouncing related functions
    bool bounceAddr(PacketPtr pkt);
    std::vector<PacketPtr> genDestReads(PacketPtr pkt);

    BouncePendingQueue m_bpq;

    Tick getCTTPenalty() { return ctt_acc_lat; }
    Tick getBPQPenalty() { return bpq_acc_lat; }

    int getMaxCTTSize() { return ctt_max_sz; }
    int getMaxBPQSize() { return bpq_max_sz; }

    struct CtrlStats : public statistics::Group
    {
        CtrlStats(MCSquare &ctrl);

        void regStats() override;

        MCSquare &ctrl;

        // All statistics that the model needs to capture
        statistics::Scalar maxEntries;
        statistics::Scalar sizeElided;
        statistics::Scalar destReadSizeCPU;
        statistics::Scalar destWriteSizeCPU;
        statistics::Scalar srcReadSizeCPU;
        statistics::Scalar srcWriteSizeCPU;
        statistics::Scalar destReadSizeBounce;
        statistics::Scalar destWriteSizeBounce;
        statistics::Scalar srcReadSizeBounce;
        statistics::Scalar srcWriteSizeBounce;
        statistics::Scalar srcWritesBlocked;
        statistics::Scalar memElideBlockedCTTFull;
    } stats;
};

} // namespace memory
} // namespace gem5

#endif //__MCSQUARE_H__

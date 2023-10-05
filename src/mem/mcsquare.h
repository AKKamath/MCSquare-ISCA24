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

    const int bt_max_sz;
    const Tick bt_acc_lat;
    class BounceTable {
      struct BounceTableEntry {
        Addr addr;
        bool writeLock;
        bool readLock;
        uint8_t data[64];
        BounceTableEntry() {
          addr = 0;
          writeLock = readLock = false;
        }
        BounceTableEntry(Addr addr, bool wLock, bool rLock, uint8_t *data) {
          this->addr = addr;
          this->writeLock = wLock;
          this->readLock = rLock;
          memcpy(this->data, data, 64);
        }
      };
      std::map<Addr, BounceTableEntry> m_table;

    public:
      void insert(Addr addr, uint8_t *data, bool wLock = false, bool rLock = false) {
        m_table[addr] = BounceTableEntry(addr, wLock, rLock, data);
      }

      void remove(Addr addr) {
        m_table.erase(addr);
      }

      Addr find(Addr addr) {
        if(m_table.find(addr) != m_table.end())
          return m_table[addr].addr;
        return 0;
      }

      bool getWriteLocked(Addr addr) {
        if(m_table.find(addr) == m_table.end())
          return false;
        return m_table[addr].writeLock;
      }

      bool getReadLocked(Addr addr) {
        if(m_table.find(addr) == m_table.end())
          return false;
        return m_table[addr].readLock;
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

  public:
    enum Types {
      TYPE_NONE = 0,
      TYPE_SRC,
      TYPE_DEST,
    };

    MCSquare(const MCSquareParams &params) : SimObject(params),
      ctt_max_sz(params.ctt_size), ctt_acc_lat(params.ctt_penalty), 
      bt_max_sz(params.bt_size), bt_acc_lat(params.bt_penalty),
      stats(*this) {
        printf("Created MCSquare with: CTT (%d size, %lu ticks); ", ctt_max_sz, ctt_acc_lat);
        printf("BT(%d size, %lu ticks)\n", bt_max_sz, bt_acc_lat);
      }

    // CTT management functions
    void insertEntry(Addr dest, Addr src, uint64_t size);
    void deleteEntry(Addr dest, uint64_t size);
    void splitEntry(PacketPtr pkt);
    Types contains(Addr addr, size_t size);
    Types contains(PacketPtr pkt);

    // Bouncing related functions
    bool bounceAddr(PacketPtr pkt);
    std::vector<PacketPtr> genDestReads(PacketPtr pkt);

    BounceTable m_bt;

    Tick getCTTPenalty() { return ctt_acc_lat; }
    Tick getBTPenalty() { return bt_acc_lat; }

    int getMaxCTTSize() { return ctt_max_sz; }
    int getMaxBTSize() { return bt_max_sz; }

    struct CtrlStats : public statistics::Group
    {
        CtrlStats(MCSquare &ctrl);

        void regStats() override;

        MCSquare &ctrl;

        // All statistics that the model needs to capture
        statistics::Scalar maxEntries;
        statistics::Scalar sizeElided;
        statistics::Scalar destReadSize;
        statistics::Scalar destWriteSize;
        statistics::Scalar srcReadSize;
        statistics::Scalar srcWriteSize;
        statistics::Scalar srcWritesBlocked;
    };

    CtrlStats stats;
};

} // namespace memory
} // namespace gem5

#endif //__MCSQUARE_H__

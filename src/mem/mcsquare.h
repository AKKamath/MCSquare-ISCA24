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

namespace gem5
{

// Helpers to simplify code
bool isMCSquare(RequestPtr req);
bool isMCSquare(PacketPtr pkt);

namespace memory
{

class MemInterface;
class DRAMInterface;
class NVMInterface;

class MCSquare {
    struct TableEntry {
      Addr dest;
      Addr src;
      uint64_t size;
      uint32_t access_ctr;

      TableEntry() {}
      TableEntry(Addr dest, Addr src, uint64_t size) {
        this->dest = dest;
        this->src = src;
        this->size = size;
        this->access_ctr = 0;
      }
    };

    std::list<TableEntry> m_table;

  public:
    enum Types {
      TYPE_NONE = 0,
      TYPE_SRC,
      TYPE_DEST,
    };

    void insertEntry(Addr dest, Addr src, uint64_t size);
    void deleteEntry(Addr dest, uint64_t size);
    void splitEntry(Addr splitAddr, uint64_t size);
    Types contains(PacketPtr pkt);
};

} // namespace memory
} // namespace gem5

#endif //__MCSQUARE_H__

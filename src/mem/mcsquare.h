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
    const int max_tbl_sz;
    const Tick tbl_acc_lat;

  public:
    enum Types {
      TYPE_NONE = 0,
      TYPE_SRC,
      TYPE_DEST,
    };

    MCSquare(const MCSquareParams &params) : SimObject(params),
      max_tbl_sz(params.table_size), tbl_acc_lat(params.table_penalty) {
        printf("Created MCSquare with %d size for %lu tiks\n", max_tbl_sz, tbl_acc_lat);
      }

    void insertEntry(Addr dest, Addr src, uint64_t size);
    void deleteEntry(Addr dest, uint64_t size);
    void splitEntry(Addr splitAddr, uint64_t size);
    Types contains(PacketPtr pkt);

    Tick getPenalty() { return tbl_acc_lat; }
};

} // namespace memory
} // namespace gem5

#endif //__MCSQUARE_H__

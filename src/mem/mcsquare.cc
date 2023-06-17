#include "mcsquare.h"


namespace gem5
{

bool isMCSquare(const RequestPtr req)
{
    return req->getFlags() & Request::MEM_ELIDE ||
           req->getFlags() & Request::MEM_ELIDE_FREE;
}

bool isMCSquare(const gem5::Packet* pkt)
{
  return isMCSquare(pkt->req);
}

namespace memory
{

void
MCSquare::insertEntry(Addr dest, Addr src, uint64_t size)
{
    /*
     * TODO:
     * 1) Merge entries if the continue
     * 2) If Src matches dest of existing entry, have entry use src of other entry
     * 3) If dest exists, but new src, either split entry or rewrite entry
     * 4) If src is new dest (???)
    */
    for(auto i = m_table.begin(); i != m_table.end(); ++i) {
        if(i->src + i->size == src && i->dest + i->size == dest) {
            printf("Merged: dest - %lx, src - %lx, size - %lu\n", dest, src, size);
            i->size += size;
            return;
        }
        if(src + size == i->src && dest + size == i->dest) {
            printf("Merged: dest - %lx, src - %lx, size - %lu\n", dest, src, size);
            i->size += size;
            i->src = src;
            i->dest = dest;
            return;
        }
    }

    m_table.push_back(TableEntry(dest, src, size));
    printf("Added: dest - %lx, src - %lx, size - %lu\n", dest, src, size);
    /*printf("Now contains: \n");
    for(auto i = m_table.begin(); i != m_table.end(); ++i) {
        printf("Src: %lx - %lx, dest %lx - %lx\t", i->src, i->src + i->size, i->dest, i->dest + i->size);
    }
    printf("\n");*/
}

void
MCSquare::deleteEntry(Addr dest, uint64_t size)
{
    if(size == (uint64_t)1) {
        printf("Clearing elision table\n");
        m_table.clear();
        return;
    }
    /*
     * TODO:
     * 1) Split entry if doesn't cover entire entry
    */
    for(auto i = m_table.begin(); i != m_table.end(); ++i) {
        if(i->dest == dest && i->size == size) {
            printf("Deleted: dest - %lx, src - %lx, size - %lu\n", i->dest, i->src, i->size);
            m_table.erase(i);
            return;
        }
    }
}

void
MCSquare::splitEntry(Addr splitAddr, uint64_t size)
{
    for(auto i = m_table.begin(); i != m_table.end(); ++i) {
        if(RangeSize(i->src, i->size).contains(splitAddr)) {
            // TODO
        }
        if(RangeSize(i->dest, i->size).contains(splitAddr)) {
            // TODO
        }
    }
}

MCSquare::Types
MCSquare::contains(PacketPtr pkt)
{
    // TODO: Add dest/src info to packet header.
    for(auto i = m_table.begin(); i != m_table.end(); ++i) {
        if(pkt->getAddrRange().intersects(RangeSize(i->src, i->size))) {
            pkt->req->_paddr_dest = i->dest + (pkt->getAddr() - i->src);
            pkt->req->_paddr_src = pkt->getAddr();
            return Types::TYPE_SRC;
        }
        if(pkt->getAddrRange().intersects(RangeSize(i->dest, i->size))) {
            pkt->req->_paddr_dest = pkt->getAddr();
            pkt->req->_paddr_src = i->src + (pkt->getAddr() - i->dest);
            return Types::TYPE_DEST;
        }
    }
    return Types::TYPE_NONE;
}

} // namespace memory
} // namespace gem5
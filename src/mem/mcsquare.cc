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
    if(size <= 0)
        return;

    /*
     * TODO:
     * 1) Merge entries if they continue -> DONE
     * 2) If Src matches dest of existing entry, have entry use src of other entry
     * 3) If dest exists, but new src, either split entry or rewrite entry
     * 4) If src is new dest (???)
    */
    for(auto i = m_table.begin(); i != m_table.end(); ++i) {
        // See if we can merge entries first
        if(i->src + i->size == src && i->dest + i->size == dest) {
            printf("%s: Merged: dest - %lx, src - %lx, size - %lu\n", 
                   name().c_str(), dest, src, size);
            i->size += size;
            return;
        }
        if(src + size == i->src && dest + size == i->dest) {
            printf("%s: Merged: dest - %lx, src - %lx, size - %lu\n", 
                   name().c_str(), dest, src, size);
            i->dest = dest;
            i->src  = src;
            i->size += size;
            return;
        }

        // See if the current dest already exists
        if(RangeSize(dest, size).intersects(RangeSize(i->dest, i->size))) {
            if(dest <= i->dest) {
                // See if this entry is subsumed by this operation
                if(dest + size >= i->dest + i->size) {
                    auto entry = i;
                    i--;
                    m_table.erase(entry);
                    continue;
                } else {
                    // Just an intersection. Cut this entry down to point of intersection
                    uint64_t offset = dest + size - i->dest;
                    i->dest += offset;
                    i->src  += offset;
                    i->size -= offset;
                    continue;
                }
            } else {
                // Check if op exceeds the entry
                if(dest + size >= i->dest + i->size) {
                    // If so, cut entry to point of intersection
                    i->size = dest - i->dest;
                    continue;
                } else {
                    // Need 3 entries by the end of this. 
                    // 1. Cut down current entry to left fringe
                    // 2. Add current entry remaining right fringe.
                    // 3. New entry for current op
                    uint64_t curr_size = i->size;

                    // 1. Cut down current entry to left fringe
                    i->size = dest - i->dest;
                    
                    // 2. Add current entry remaining right fringe
                    uint64_t fringe_dest = dest + size;
                    uint64_t offset = fringe_dest - i->dest;
                    uint64_t fringe_src  = i->src + offset;
                    uint64_t fringe_size = curr_size - offset;
                    insertEntry(fringe_dest, fringe_src, fringe_size);

                    // 3. New entry for current op (done outside loop)
                    continue;
                }
            }
        }

        // See if the current src is a previous dest
        if(RangeSize(src, size).intersects(RangeSize(i->dest, i->size))) {
            if(src >= i->dest) {
                uint64_t offset = src - i->dest;
                uint64_t temp_src = i->src + offset;
                uint64_t temp_size = std::min(size, i->size - offset);

                printf("%s: Redirected src: dest - %lx, (og src %lx) "
                       "now src - %lx, size - %lu\n", 
                       name().c_str(), dest, src, temp_src, temp_size);
                m_table.push_back(TableEntry(dest, temp_src, temp_size));
                dest += temp_size;
                src  += temp_size;
                size -= temp_size;
                insertEntry(dest, src, size);
                return;
            } else {
                uint64_t offset = i->dest - src;
                insertEntry(dest, src, offset);
                dest += offset;
                src  += offset;
                size -= offset;
                --i;
                continue;
            }
        }
    }

    m_table.push_back(TableEntry(dest, src, size));
    printf("%s: Added: dest - %lx, src - %lx, size - %lu\n", 
           name().c_str(), dest, src, size);
}

void
MCSquare::deleteEntry(Addr dest, uint64_t size)
{
    if(size == (uint64_t)1) {
        printf("%s: Clearing elision table\n", name().c_str());
        m_table.clear();
        return;
    }
    // Cut down deleted portion of entry
    for(auto i = m_table.begin(); i != m_table.end(); ++i) {
        if(RangeSize(dest, size).intersects(RangeSize(i->dest, i->size))) {
            if(dest <= i->dest) {
                // See if this entry is subsumed by this operation
                if(dest + size >= i->dest + i->size) {
                    printf("%s: Deleted: dest - %lx, size - %lu\n", 
                        name().c_str(), i->dest, i->size);
                    auto entry = i;
                    i--;
                    m_table.erase(entry);
                    continue;
                } else {
                    // Just an intersection. Cut this entry down to point of intersection
                    uint64_t offset = dest + size - i->dest;
                    i->dest += offset;
                    i->src  += offset;
                    i->size -= offset;
                    printf("%s: Downsized1: dest - %lx, size - %lu\n", 
                        name().c_str(), i->dest, i->size);
                    continue;
                }
            } else {
                // Check if op exceeds the entry
                if(dest + size >= i->dest + i->size) {
                    // If so, cut entry to point of intersection
                    i->size = dest - i->dest;
                    printf("%s: Downsized2: dest - %lx, size - %lu\n", 
                        name().c_str(), i->dest, i->size);
                    continue;
                } else {
                    // Need to split into 2 disjoint entries
                    // 1. Cut down current entry to left fringe
                    // 2. Add current entry remaining right fringe.
                    uint64_t curr_size = i->size;

                    // 1. Cut down current entry to left fringe
                    i->size = dest - i->dest;
                    printf("%s: Downsized3: dest - %lx, size - %lu\n", 
                        name().c_str(), i->dest, i->size);
                    
                    // 2. Add current entry remaining right fringe
                    uint64_t fringe_dest = dest + size;
                    uint64_t offset = fringe_dest - i->dest;
                    uint64_t fringe_src  = i->src + offset;
                    uint64_t fringe_size = curr_size - offset;
                    insertEntry(fringe_dest, fringe_src, fringe_size);
                    continue;
                }
            }
        }
    }
}

void
MCSquare::splitEntry(PacketPtr pkt)
{
    assert(pkt->getSize() == 64);
    assert((pkt->getAddr() & 63) == 0);
    gem5::Addr splitAddr = pkt->getAddr();
    // Delete entry will automatically split
    deleteEntry(pkt->getAddr(), pkt->getSize());
    // Now see what to do for src
    for(auto i = m_table.begin(); i != m_table.end(); ++i) {
        if(RangeSize(i->src, i->size).contains(splitAddr)) {
            // TODO
            fflush(stdout);
            assert(false);
        }
    }
}

MCSquare::Types
MCSquare::contains(PacketPtr pkt)
{
    for(auto i = m_table.begin(); i != m_table.end(); ++i) {
        if(pkt->getAddrRange().intersects(RangeSize(i->src, i->size))) {
            if(pkt->req->_paddr_src == 0)
                pkt->req->_paddr_src = pkt->getAddr();
            //printf("Packet (%lx, %lu) intersects src entry: "
            //    "dest %lx, src %lx, size %lu\n", pkt->getAddr(), pkt->getSize(),
            //    i->dest, i->src, i->size);
            return Types::TYPE_SRC;
        }
    }
    for(auto i = m_table.begin(); i != m_table.end(); ++i) {
        if(pkt->getAddrRange().intersects(RangeSize(i->dest, i->size))) {
            if(pkt->req->_paddr_dest == 0)
                pkt->req->_paddr_dest = pkt->getAddr();
            printf("Packet (%lx, %u) intersects entry: "
                "dest %lx, src %lx, size %lu\n", pkt->getAddr(), pkt->getSize(),
                i->dest, i->src, i->size);
            return Types::TYPE_DEST;
        }
    }
    return Types::TYPE_NONE;
}

bool
MCSquare::bounceAddr(PacketPtr pkt)
{
    // First move dest_offset to appropriate position
    if(pkt->mc_dest_offset == -1)
        pkt->mc_dest_offset = 0;
    else
        pkt->mc_dest_offset += pkt->mc_size;
    
    // Done all bouncing. Return to original form.
    if(pkt->mc_dest_offset == 64) {
        pkt->setAddr(pkt->req->_paddr_dest);
        return true;
    }

    for(auto i = m_table.begin(); i != m_table.end(); ++i) {
        if(RangeSize(i->dest, i->size).contains(pkt->req->_paddr_dest + pkt->mc_dest_offset)) {
            uint64_t offset = pkt->req->_paddr_dest + pkt->mc_dest_offset - i->dest;
            // Convert to src address to bounce
            pkt->setAddr((i->src + offset) & ~(63));
            pkt->mc_src_offset = ((i->src + offset) & (63));
            pkt->mc_size = std::min(64 - pkt->mc_dest_offset, 
                std::min(i->size - offset, 64 - pkt->mc_src_offset));
            return false;
        }
    }

    // Shouldn't reach here
    fflush(stdout);
    assert(false);
    return true;
}

} // namespace memory
} // namespace gem5
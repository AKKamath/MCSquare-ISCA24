#include "mcsquare.h"


namespace gem5
{

bool isMCSquare(const RequestPtr req)
{
    if(req && req != NULL)
    return req->getFlags() & Request::MEM_ELIDE ||
           req->getFlags() & Request::MEM_ELIDE_FREE;
    return false;
}

bool isMCSquare(const gem5::Packet* pkt)
{
    if(!pkt)
        return false;
    return isMCSquare(pkt->req);
}

bool isMCReq(const gem5::Packet* pkt)
{
    bool sourceBounce =  pkt->req->getFlags() & 
        (Request::MEM_ELIDE_WRITE_SRC | Request::MEM_ELIDE_REDIRECT_SRC);
    bool destWB =  pkt->req->getFlags() & (Request::MEM_ELIDE_DEST_WB);
    return sourceBounce || destWB;
}

namespace memory
{

void
MCSquare::insertEntry(Addr dest, Addr src, uint64_t size)
{
    if(size <= 0)
        return;

    /*
     * Steps for insertion:
     * 1) If dest exists in an entry, either split or remove existing entry
     * 2) If src matches dest of existing entry, use src of existing entry
     * 3) Merge entries if they are contiguous
     * 4) Finally, insert new entry
     */

    // 1) See if the current dest already exists
    for(auto i = m_ctt.begin(); i != m_ctt.end(); ++i) {
        if(RangeSize(dest, size).intersects(RangeSize(i->dest, i->size))) {
            if(dest <= i->dest) {
                // See if this entry is subsumed by this operation
                if(dest + size >= i->dest + i->size) {
                    DPRINTF(MCSquare, "New entry (%lx, %lx, %lu) intersects. Removing this entry (%lx, %lx, %lu)\n", 
                        dest, src, size, i->dest, i->src, i->size);
                    i = m_ctt.erase(i);
                    i--;
                    continue;
                } else {
                    // Just an intersection. Cut this entry down to point of intersection
                    uint64_t offset = dest + size - i->dest;
                    i->dest += offset;
                    i->src  += offset;
                    i->size -= offset;
                    DPRINTF(MCSquare, "New entry (%lx, %lx, %lu) intersects. Cutting down this entry (%lx, %lx, %lu)\n", 
                        dest, src, size, i->dest, i->src, i->size);
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
                    m_ctt.push_back(CTTableEntry(fringe_dest, fringe_src, fringe_size));

                    // 3. New entry for current op (done outside loop)
                    continue;
                }
            }
        }
    }

    // 2) If the current src is a previous dest, use previous src
    for(auto i = m_ctt.begin(); i != m_ctt.end(); ++i) {
        if(RangeSize(src, size).intersects(RangeSize(i->dest, i->size))) {
            if(src >= i->dest) {
                uint64_t offset = src - i->dest;
                uint64_t temp_src = i->src + offset;
                uint64_t temp_size = std::min(size, i->size - offset);

                DPRINTF(MCSquare, "Redirected src: dest - %lx, (og src %lx) "
                       "now src - %lx, size - %lu\n", 
                       dest, src, temp_src, temp_size);
                /*if(dest == temp_src && 
                    (temp_size > 64 || (dest % 64 == 0 && temp_size == 64))) {
                    uint64_t temp_size2 = 64 - (dest % 64);
                    if(temp_size2 != 64) {
                        m_ctt.push_back(CTTableEntry(dest, temp_src, temp_size2));
                        DPRINTF(MCSquare, "Added: dest - %lx, src - %lx, size - %lu\n", 
                                dest, temp_src, temp_size2);
                        dest      += temp_size2;
                        temp_src  += temp_size2;
                        src       += temp_size2;
                        temp_size -= temp_size2;
                        size -= temp_size2;
                    }
                    uint64_t temp_size3 = temp_size % 64;
                    dest      += temp_size - temp_size3;
                    temp_src  += temp_size - temp_size3;
                    src       += temp_size - temp_size3;
                    temp_size -= temp_size - temp_size3;
                    size      -= temp_size - temp_size3;
                    if(temp_size3 > 0) {
                        m_ctt.push_back(CTTableEntry(dest, temp_src, temp_size3));
                        DPRINTF(MCSquare, "Added: dest - %lx, src - %lx, size - %lu\n", 
                                dest, temp_src, temp_size3);
                        dest      += temp_size3;
                        temp_src  += temp_size3;
                        src       += temp_size3;
                        temp_size -= temp_size3;
                        size      -= temp_size3;
                    }

                }
                else {*/
                    m_ctt.push_back(CTTableEntry(dest, temp_src, temp_size));
                //}
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

    // 3) See if we can merge entries
    for(auto i = m_ctt.begin(); i != m_ctt.end(); ++i) {
        if(i->src + i->size == src && i->dest + i->size == dest) {
            i->size += size;
            DPRINTF(MCSquare, "Merged: dest - %lx, src - %lx, size - %lu\n", 
                    dest, src, size);
            return;
        }
        if(src + size == i->src && dest + size == i->dest) {
            i->dest = dest;
            i->src  = src;
            i->size += size;
            DPRINTF(MCSquare, "Merged: dest - %lx, src - %lx, size - %lu\n", 
                   dest, src, size);
            return;
        }
    }

    // 4) Final insert
    m_ctt.push_back(CTTableEntry(dest, src, size));
    stats.maxEntries = std::max((size_t)stats.maxEntries.value(), m_ctt.size());
    DPRINTF(MCSquare, "Added: dest - %lx, src - %lx, size - %lu\n", 
           dest, src, size);
}

void
MCSquare::deleteEntry(Addr dest, uint64_t size)
{
    // Shortcut to reset the CTT in between process runs
    if(size == (uint64_t)1) {
        DPRINTF(MCSquare, "Clearing elision table of %ld entries\n", m_ctt.size());
        m_ctt.clear();
        return;
    }
    // Cut down deleted portion of entry
    for(auto i = m_ctt.begin(); i != m_ctt.end(); ++i) {
        if(RangeSize(dest, size).intersects(RangeSize(i->dest, i->size))) {
            if(dest <= i->dest) {
                // See if this entry is subsumed by this operation
                if(dest + size >= i->dest + i->size) {
                    DPRINTF(MCSquare, "Deleted: dest - %lx, size - %lu\n", 
                        i->dest, i->size);
                    i = m_ctt.erase(i);
                    i--;
                    continue;
                } else {
                    // Just an intersection. Cut this entry down to point of intersection
                    uint64_t offset = dest + size - i->dest;
                    i->dest += offset;
                    i->src  += offset;
                    i->size -= offset;
                    DPRINTF(MCSquare, "Downsized1: dest - %lx, size - %lu\n", 
                        i->dest, i->size);
                    continue;
                }
            } else {
                // Check if op exceeds the entry
                if(dest + size >= i->dest + i->size) {
                    // If so, cut entry to point of intersection
                    i->size = dest - i->dest;
                    DPRINTF(MCSquare, "Downsized2: dest - %lx, size - %lu\n", 
                        i->dest, i->size);
                    continue;
                } else {
                    // Need to split into 2 disjoint entries
                    // 1. Cut down current entry to left fringe
                    // 2. Add current entry remaining right fringe.
                    uint64_t curr_size = i->size;

                    // 1. Cut down current entry to left fringe
                    i->size = dest - i->dest;
                    DPRINTF(MCSquare, "Downsized3: dest - %lx, size - %lu\n", 
                        i->dest, i->size);
                    
                    // 2. Add current entry remaining right fringe
                    uint64_t fringe_dest = dest + size;
                    uint64_t offset = fringe_dest - i->dest;
                    uint64_t fringe_src  = i->src + offset;
                    uint64_t fringe_size = curr_size - offset;
                    DPRINTF(MCSquare, "Insert3: dest - %lx, src - %lx, size - %lu\n", 
                        fringe_dest, fringe_src, fringe_size);
                    m_ctt.push_back(CTTableEntry(fringe_dest, fringe_src, fringe_size));
                    continue;
                }
            }
        }
    }
}

void
MCSquare::splitEntry(PacketPtr pkt)
{
    assert(pkt->getSize() == 64 || pkt->req->getFlags() & Request::UNCACHEABLE);
    assert((pkt->getAddr() & 63) == 0 || pkt->req->getFlags() & Request::UNCACHEABLE);
    // Delete entry will automatically split
    deleteEntry(pkt->getAddr(), pkt->getSize());
}

Addr 
MCSquare::getAddrToFree(AddrRangeList addrList) 
{
    Addr minAddr = 0;
    uint64_t minSize = 0;
    for(auto i = m_ctt.begin(); i != m_ctt.end(); ++i) {
        for(auto j = addrList.begin(); j != addrList.end(); ++j)
            if(j->contains(i->src & ~((uint64_t)63)) && 
                ctt_freeing.find(i->src & ~((uint64_t)63)) == ctt_freeing.end()) {
                if(i->size < minSize || minSize == 0) {
                    minAddr = i->src & ~((uint64_t)63);
                    minSize = i->size;
                    if(minSize <= 64)
                        return minAddr;
                }
                break;
            }
    }
    return minAddr;
}

MCSquare::Types
MCSquare::contains(Addr addr, size_t size)
{
    for(auto i = m_ctt.begin(); i != m_ctt.end(); ++i) {
        if(RangeSize(addr, size).intersects(RangeSize(i->src, i->size))) {
            //DPRINTF(MCSquare, "BPQ entry %lx intersects (d%lx, s%lx, %lu)\n", 
            //        addr, i->dest, i->src, i->size);
            return Types::TYPE_SRC;
        }
    }
    for(auto i = m_ctt.begin(); i != m_ctt.end(); ++i) {
        if(RangeSize(addr, size).intersects(RangeSize(i->dest, i->size))) {
            return Types::TYPE_DEST;
        }
    }
    return Types::TYPE_NONE;
}

MCSquare::Types
MCSquare::contains(PacketPtr pkt)
{
    for(auto i = m_ctt.begin(); i != m_ctt.end(); ++i) {
        if(pkt->getAddrRange().intersects(RangeSize(i->src, i->size))) {
            if(pkt->req->_paddr_src == 0)
                pkt->req->_paddr_src = pkt->getAddr();
            //DPRINTF(MCSquare, "Packet (%lx, %lu) intersects src entry: "
            //    "dest %lx, src %lx, size %lu\n", pkt->getAddr(), pkt->getSize(),
            //    i->dest, i->src, i->size);
            return Types::TYPE_SRC;
        }
    }
    for(auto i = m_ctt.begin(); i != m_ctt.end(); ++i) {
        if(pkt->getAddrRange().intersects(RangeSize(i->dest, i->size))) {
            if(pkt->req->_paddr_dest == 0)
                pkt->req->_paddr_dest = pkt->getAddr();
            DPRINTF(MCSquare, "Packet (%lx, %u) intersects entry: "
                "dest %lx, src %lx, size %lu\n", pkt->getAddr(), pkt->getSize(),
                i->dest, i->src, i->size);
            return Types::TYPE_DEST;
        }
    }
    return Types::TYPE_NONE;
}

bool
MCSquare::isSrc(PacketPtr pkt)
{
    for(auto i = m_ctt.begin(); i != m_ctt.end(); ++i) {
        if(pkt->getAddrRange().intersects(RangeSize(i->src, i->size))) {
            return true;
        }
    }
    return false;
}

bool
MCSquare::isDest(PacketPtr pkt)
{
    for(auto i = m_ctt.begin(); i != m_ctt.end(); ++i) {
        if(pkt->getAddrRange().intersects(RangeSize(i->dest, i->size))) {
            DPRINTF(MCSquare, "Packet (%lx, %u) intersects entry: "
                "dest %lx, src %lx, size %lu\n", pkt->getAddr(), pkt->getSize(),
                i->dest, i->src, i->size);
            return true;
        }
    }
    return false;
}

bool
MCSquare::bounceAddr(PacketPtr pkt)
{
    // First move dest_offset to appropriate position
    if(pkt->mc_dest_offset == -1 || pkt->mc_dest_offset == (uint64_t)18446744073709551615)
        pkt->mc_dest_offset = 0;
    else
        pkt->mc_dest_offset += pkt->mc_size;
    
    // Done all bouncing. Return to original form.
    if(pkt->mc_dest_offset == pkt->getSize()) {
        pkt->setAddr(pkt->req->_paddr_dest);
        return true;
    } else if(pkt->mc_right_offset != -1 && 
              pkt->mc_dest_offset + pkt->mc_right_offset == pkt->getSize()) {
        pkt->setAddr(pkt->req->_paddr_dest);
        return true;
    }

    // Shouldn't reach here
    if(pkt->mc_dest_offset > 64) {
        fprintf(stderr, "%lu: Cannot find bounce entry for packet: dest=%lx, "
            "dest offset = %lu, mc_size = %lu\n", ::gem5::curTick(), 
            pkt->req->_paddr_dest, pkt->mc_dest_offset, pkt->mc_size);
        fflush(stdout);
        assert(false);
    }

    uint64_t min_intersect = (uint64_t)18446744073709551615;
    for(auto i = m_ctt.begin(); i != m_ctt.end(); ++i) {
        // Check if the next address is contained in the entry
        if(RangeSize(i->dest, i->size).contains(pkt->req->_paddr_dest + pkt->mc_dest_offset)) {
            uint64_t offset = pkt->req->_paddr_dest + pkt->mc_dest_offset - i->dest;
            // Convert to src address to bounce
            pkt->setAddr((i->src + offset) & ~(63));
            pkt->mc_src_offset = ((i->src + offset) & (63));
            pkt->mc_size = std::min(64 - pkt->mc_dest_offset, 
                std::min(i->size - offset, 64 - pkt->mc_src_offset));
            
            DPRINTF(MCSquare, "Bounce dest %lx offset = %lu, size = %lu, addr = %lx\n", 
                pkt->req->_paddr_dest, pkt->mc_dest_offset, pkt->mc_size, pkt->getAddr());
            return false;
        }
        // Check if the entire read request intersects with the entry
        // May require a partial dest read first, before we bounce src
        if(RangeSize(i->dest, i->size).intersects(
           RangeSize(pkt->req->_paddr_dest + pkt->mc_dest_offset, 
           pkt->getSize() - pkt->mc_dest_offset))) {
            if(pkt->req->_paddr_dest + pkt->mc_dest_offset >= i->dest) {
                fprintf(stderr, "%lu: Packet: dest=%lx, size=%lu; "
                    "Entry dest=%lx,entry size=%lu\n dest offset = %lu\n", 
                    ::gem5::curTick(), pkt->req->_paddr_dest, pkt->getSize(), 
                    i->dest, i->size, pkt->mc_dest_offset);
            }
            assert(pkt->req->_paddr_dest + pkt->mc_dest_offset < i->dest);
            min_intersect = std::min(min_intersect, 
                (uint64_t)i->dest - (pkt->req->_paddr_dest + pkt->mc_dest_offset));
        }
    }

    // Require partial dest read first, before src bounce
    if(min_intersect != (uint64_t)18446744073709551615) {
        pkt->setAddr(pkt->req->_paddr_dest);
        pkt->mc_src_offset = pkt->mc_dest_offset;
        pkt->mc_size = std::min(min_intersect, 64 - pkt->mc_src_offset);
        return false;
    }

    // Bounced dest read for already written dest
    if(pkt->req->getFlags() & Request::MEM_ELIDE_WRITE_SRC) {
        pkt->setAddr(pkt->req->_paddr_dest);
        pkt->mc_dest_offset = 0;
        pkt->mc_size = 0;
        return true;
    }
    
    fprintf(stderr, "%lu: Cannot find bounce entry for packet: dest=%lx, "
        "dest offset = %lu\n", ::gem5::curTick(), pkt->req->_paddr_dest, pkt->mc_dest_offset);
    //assert(false);
    // Dest was written while a bounce was ongoing.
    pkt->setAddr(pkt->req->_paddr_dest);
    pkt->mc_dest_offset = 0;
    pkt->mc_size = 0;
    pkt->req->_paddr_dest = 100;
    return false;
}

std::vector<PacketPtr> 
MCSquare::genDestReads(PacketPtr srcPkt) 
{
    std::vector<PacketPtr> destReads;
    for(auto i = m_ctt.begin(); i != m_ctt.end(); ++i) {
        if(RangeSize(i->src, i->size).intersects(
                RangeSize(srcPkt->getAddr(), srcPkt->getSize()))) {
            int64_t offset = srcPkt->getAddr() - i->src;
            // For rare, irregular case where dest is not cache-aligned
            int64_t doffset = i->dest % 64;

            DPRINTF(MCSquare, "Gen read offset = %ld, doffset=%ld, srcpkt addr: %lx;  "
                    "Entry intersect: dest %lx, src %lx, size %lu; peek: %lu\n",
                    offset, doffset, srcPkt->getAddr(), i->dest, i->src, i->size, *srcPkt->getPtr<uint64_t>());
            
            if(offset >= 0) {
                // Should intersect 2 dest cachelines in this case
                if((offset + doffset) % 64 > 0 && i->size - offset >= 64) {
                    // How it looks in memory relative to entry:
                    // DEST: {         [DEST1][DEST2]      }
                    // SRC:  {<---offset-->[ SRC ]         }
                    
                    // Create packet for DEST1:
                    {
                        Addr destAddr = (i->dest + offset) & ~(63lu);
                        auto req = std::make_shared<Request>(destAddr, srcPkt->getSize(), 
                            Request::MEM_ELIDE_WRITE_SRC | Request::MEM_ELIDE_REDIRECT_SRC, 
                            srcPkt->req->funcRequestorId);
                        auto pkt = Packet::createRead(req);
                        pkt->req->_paddr_dest = destAddr;
                        pkt->allocate();
                        bounceAddr(pkt);
                        pkt->setData(srcPkt->getPtr<uint8_t>(), (offset + doffset) % 64, 
                                    0, 64 - ((offset + doffset) % 64));
                        pkt->mc_right_offset = 64 - ((doffset + offset) % 64);
                        destReads.push_back(pkt);
                    }
                    // Create packet for DEST2:
                    {
                        Addr destAddr = (i->dest + offset + 64) & ~(63lu);
                        auto req = std::make_shared<Request>(destAddr, srcPkt->getSize(), 
                            Request::MEM_ELIDE_WRITE_SRC | Request::MEM_ELIDE_REDIRECT_SRC, 
                            srcPkt->req->funcRequestorId);
                        auto pkt = Packet::createRead(req);
                        pkt->req->_paddr_dest = destAddr;
                        pkt->allocate();
                        bounceAddr(pkt);
                        pkt->setData(srcPkt->getPtr<uint8_t>(), pkt->mc_dest_offset, 
                             pkt->mc_src_offset, pkt->mc_size);
                        bounceAddr(pkt);
                        destReads.push_back(pkt);
                    }
                } else if((offset + doffset) % 64 == 0 && i->size - offset >= 64) {
                    // Intersects only 1 dest
                    // How it looks in memory relative to entry:
                    // DEST: {             [DEST ]         }
                    // SRC:  {<---offset-->[ SRC ]         }
                    Addr destAddr = (i->dest + offset) & ~(63lu);
                    auto req = std::make_shared<Request>(destAddr, srcPkt->getSize(), 
                        Request::MEM_ELIDE_WRITE_SRC | Request::MEM_ELIDE_REDIRECT_SRC, 
                        srcPkt->req->funcRequestorId);
                    auto pkt = Packet::createRead(req);
                    pkt->req->_paddr_dest = destAddr;
                    pkt->allocate();
                    bounceAddr(pkt);
                    pkt->setData(srcPkt->getPtr<uint8_t>(), pkt->mc_dest_offset, 
                            pkt->mc_src_offset, pkt->mc_size);
                    bounceAddr(pkt);
                    assert(pkt->mc_dest_offset == 64);
                    pkt->cmd = pkt->makeWriteCmd(pkt->req);
                    destReads.push_back(pkt);
                } else if((doffset + offset) % 64 == 0 && i->size - offset < 64) {
                    // Intersects only 1 dest
                    // How it looks in memory relative to entry:
                    // DEST: {             [DEST ]         }
                    // SRC:  {<---offset-->[ S]         }
                    Addr destAddr = (i->dest + offset) & ~(63lu);
                    auto req = std::make_shared<Request>(destAddr, srcPkt->getSize(), 
                        Request::MEM_ELIDE_WRITE_SRC | Request::MEM_ELIDE_REDIRECT_SRC, 
                        srcPkt->req->funcRequestorId);
                    auto pkt = Packet::createRead(req);
                    pkt->req->_paddr_dest = destAddr;
                    pkt->allocate();
                    bounceAddr(pkt);
                    pkt->setData(srcPkt->getPtr<uint8_t>(), pkt->mc_dest_offset, 
                            pkt->mc_src_offset, pkt->mc_size);
                    bounceAddr(pkt);
                    destReads.push_back(pkt);
                } else if((doffset + offset) % 64 > 0 && i->size - offset < 64) {
                    // Intersects only 1 dest at end
                    // How it looks in memory relative to entry:
                    // DEST: {         [DEST ]}
                    // SRC:  {<---offset-->[ S}RC ]
                    Addr destAddr = (i->dest + offset) & ~(63lu);
                    auto req = std::make_shared<Request>(destAddr, srcPkt->getSize(), 
                        Request::MEM_ELIDE_WRITE_SRC | Request::MEM_ELIDE_REDIRECT_SRC, 
                        srcPkt->req->funcRequestorId);
                    auto pkt = Packet::createRead(req);
                    pkt->req->_paddr_dest = destAddr;
                    pkt->allocate();
                    bounceAddr(pkt);
                    pkt->setData(srcPkt->getPtr<uint8_t>(), (doffset + offset) % 64, 
                                0, 64 - ((doffset + offset) % 64));
                    pkt->mc_right_offset = 64 - ((doffset + offset) % 64);
                    destReads.push_back(pkt);
                } else {
                    fprintf(stderr, "Trying to gen read: "
                            "offset = %ld, srcpkt addr: %lx\n"
                            "Entry intersect: dest %lx, src %lx, size %lu\n",
                            offset, srcPkt->getAddr(), i->dest, i->src, i->size);
                    assert(false);
                }
            } else {
                // Only portion of entry intersects dest:
                // How it looks in memory relative to entry:
                // DEST: {[DEST ]         }
                // SRC:[ {SRC ]           }
                //     <-> (negative offset)

                // Intersects 2 cachelines, because this dest has too little left
                if((offset + 64) + doffset > 64 && (i->size + doffset) > 64) {
                    // 1st packet
                    {
                        Addr destAddr =  (i->dest) & ~(63lu);
                        auto req = std::make_shared<Request>(destAddr, srcPkt->getSize(), 
                            Request::MEM_ELIDE_WRITE_SRC | Request::MEM_ELIDE_REDIRECT_SRC, 
                            srcPkt->req->funcRequestorId);
                        auto pkt = Packet::createRead(req);
                        pkt->req->_paddr_dest = destAddr;
                        pkt->allocate();
                        bounceAddr(pkt);
                        uint64_t amount = 64 - doffset;
                        pkt->setData(srcPkt->getPtr<uint8_t>(), doffset, 
                                    -1 * offset, amount);
                        pkt->mc_right_offset = 64 - (amount % 64);
                        destReads.push_back(pkt);
                    }
                    // 2nd packet
                    {
                        Addr destAddr =  (i->dest + 64) & ~(63lu);
                        auto req = std::make_shared<Request>(destAddr, srcPkt->getSize(), 
                            Request::MEM_ELIDE_WRITE_SRC | Request::MEM_ELIDE_REDIRECT_SRC, 
                            srcPkt->req->funcRequestorId);
                        auto pkt = Packet::createRead(req);
                        pkt->req->_paddr_dest = destAddr;
                        pkt->allocate();
                        bounceAddr(pkt);
                        pkt->setData(srcPkt->getPtr<uint8_t>(), pkt->mc_dest_offset, 
                                pkt->mc_src_offset, pkt->mc_size);
                        bounceAddr(pkt);
                        destReads.push_back(pkt);
                    }
                } else if((offset + 64) + doffset >= 64) {
                    Addr destAddr =  (i->dest) & ~(63lu);
                    auto req = std::make_shared<Request>(destAddr, srcPkt->getSize(), 
                        Request::MEM_ELIDE_WRITE_SRC | Request::MEM_ELIDE_REDIRECT_SRC, 
                        srcPkt->req->funcRequestorId);
                    auto pkt = Packet::createRead(req);
                    pkt->req->_paddr_dest = destAddr;
                    pkt->allocate();
                    bounceAddr(pkt);
                    uint64_t amount = std::min((uint64_t)(64 + offset), i->size);
                    pkt->setData(srcPkt->getPtr<uint8_t>(), doffset, 
                                -1 * offset, amount);
                    pkt->mc_right_offset = amount;
                    destReads.push_back(pkt);
                } else if(doffset == 0) {
                        Addr destAddr =  (i->dest) & ~(63lu);
                        auto req = std::make_shared<Request>(destAddr, srcPkt->getSize(), 
                            Request::MEM_ELIDE_WRITE_SRC | Request::MEM_ELIDE_REDIRECT_SRC, 
                            srcPkt->req->funcRequestorId);
                        auto pkt = Packet::createRead(req);
                        pkt->req->_paddr_dest = destAddr;
                        pkt->allocate();
                        bounceAddr(pkt);
                        pkt->setData(srcPkt->getPtr<uint8_t>(), pkt->mc_dest_offset, 
                                pkt->mc_src_offset, pkt->mc_size);
                        bounceAddr(pkt);
                        destReads.push_back(pkt);
                } else {
                    Addr destAddr =  (i->dest) & ~(63lu);
                    auto req = std::make_shared<Request>(destAddr, srcPkt->getSize(), 
                        Request::MEM_ELIDE_WRITE_SRC | Request::MEM_ELIDE_REDIRECT_SRC, 
                        srcPkt->req->funcRequestorId);
                    auto pkt = Packet::createRead(req);
                    pkt->req->_paddr_dest = destAddr;
                    pkt->allocate();
                    bounceAddr(pkt);
                    destReads.push_back(pkt);
                    fprintf(stderr, "Trying to gen read: "
                            "offset = %ld, doffset = %ld, srcpkt addr: %lx\n"
                            "Entry intersect: dest %lx, src %lx, size %lu\n",
                            offset, doffset, srcPkt->getAddr(), i->dest, i->src, i->size);
                }
            }
        }
    }
    return destReads;
}

MCSquare::CtrlStats::CtrlStats(MCSquare &_ctrl)
    : statistics::Group(&_ctrl),
    ctrl(_ctrl),
    ADD_STAT(maxEntries, statistics::units::Count::get(),
             "Maximum size of elision table during simulation"),
    ADD_STAT(sizeElided, statistics::units::Count::get(),
             "Total size (in bytes) of data elided"),
    ADD_STAT(destReadSizeCPU, statistics::units::Count::get(),
             "Amount (in bytes) of destination data read by CPU"),
    ADD_STAT(destWriteSizeCPU, statistics::units::Count::get(),
             "Amount (in bytes) of destination data written by CPU"),
    ADD_STAT(srcReadSizeCPU, statistics::units::Count::get(),
             "Amount (in bytes) of src data read by CPU"),
    ADD_STAT(srcWriteSizeCPU, statistics::units::Count::get(),
             "Amount (in bytes) of src data written by CPU"),
    ADD_STAT(destReadSizeBounce, statistics::units::Count::get(),
             "Amount (in bytes) of destination data read by bounce"),
    ADD_STAT(destWriteSizeBounce, statistics::units::Count::get(),
             "Amount (in bytes) of destination data written by bounce"),
    ADD_STAT(srcReadSizeBounce, statistics::units::Count::get(),
             "Amount (in bytes) of src data read by bounce"),
    ADD_STAT(srcWriteSizeBounce, statistics::units::Count::get(),
             "Amount (in bytes) of src data written by bounce"),
    ADD_STAT(srcWritesBlocked, statistics::units::Count::get(),
             "Number of writes to src blocked"),
    ADD_STAT(memElideBlockedCTTFull, statistics::units::Count::get(),
             "Number of mem elides blocked due to CTT being full")
             
{
}

void
MCSquare::CtrlStats::regStats()
{
    using namespace statistics;
}

} // namespace memory
} // namespace gem5
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
    bool sourceBounce =  pkt->req->getFlags() & (Request::MEM_ELIDE_WRITE_SRC | Request::MEM_ELIDE_REDIRECT_SRC);
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
            DPRINTF(MCSquare, "Merged: dest - %lx, src - %lx, size - %lu\n", 
                    dest, src, size);
            i->size += size;
            return;
        }
        if(src + size == i->src && dest + size == i->dest) {
            DPRINTF(MCSquare, "Merged: dest - %lx, src - %lx, size - %lu\n", 
                   dest, src, size);
            i->dest = dest;
            i->src  = src;
            i->size += size;
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
    assert(pkt->getSize() == 64);
    assert((pkt->getAddr() & 63) == 0);
    // Delete entry will automatically sqplit
    deleteEntry(pkt->getAddr(), pkt->getSize());
}

MCSquare::Types
MCSquare::contains(Addr addr, size_t size)
{
    for(auto i = m_ctt.begin(); i != m_ctt.end(); ++i) {
        if(RangeSize(addr, size).intersects(RangeSize(i->src, i->size))) {
            DPRINTF(MCSquare, "BPQ entry %lx intersects (d%lx, s%lx, %lu)\n", 
                    addr, i->dest, i->src, i->size);
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
    if(pkt->mc_dest_offset == -1)
        pkt->mc_dest_offset = 0;
    else
        pkt->mc_dest_offset += pkt->mc_size;
    
    // Done all bouncing. Return to original form.
    if(pkt->mc_dest_offset == 64) {
        pkt->setAddr(pkt->req->_paddr_dest);
        return true;
    } else if(pkt->mc_right_offset != -1 && 
              pkt->mc_dest_offset + pkt->mc_right_offset == 64) {
        pkt->setAddr(pkt->req->_paddr_dest);
        return true;
    }

    // Shouldn't reach here
    if(pkt->mc_dest_offset > 64) {
        fprintf(stderr, "%lu: Cannot find bounce entry for packet: dest=%lx, "
            "dest offset = %lu\n", ::gem5::curTick(), pkt->req->_paddr_dest, pkt->mc_dest_offset);
        fflush(stdout);
        assert(false);
    }

    for(auto i = m_ctt.begin(); i != m_ctt.end(); ++i) {
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
    }
    // Bounced dest read for already written dest
    if(pkt->req->getFlags() & Request::MEM_ELIDE_WRITE_SRC) {
        pkt->setAddr(pkt->req->_paddr_dest);
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
    ADD_STAT(destReadSize, statistics::units::Count::get(),
             "Amount (in bytes) of destination data read"),
    ADD_STAT(destWriteSize, statistics::units::Count::get(),
             "Amount (in bytes) of destination data written"),
    ADD_STAT(srcReadSize, statistics::units::Count::get(),
             "Amount (in bytes) of src data read"),
    ADD_STAT(srcWriteSize, statistics::units::Count::get(),
             "Amount (in bytes) of src data written"),
    ADD_STAT(srcWritesBlocked, statistics::units::Count::get(),
             "Number of writes to src blocked")
{
}

void
MCSquare::CtrlStats::regStats()
{
    using namespace statistics;
}

} // namespace memory
} // namespace gem5
from m5.params import *
from m5.SimObject import SimObject

class MCSquare(SimObject):
    type = 'MCSquare'
    cxx_header = "mem/mcsquare.h"
    cxx_class = "gem5::memory::MCSquare"

    ctt_size = Param.Int(4096, "Number of entries that copy tracking table can hold")
    ctt_penalty = Param.Latency("979ps", "Cycle penalty for reading a data copy that was elided")
    ctt_free = Param.Float(0.75, "Fraction of CTT to fill before we start freeing entries")

    bpq_size = Param.Int(4, "Number of entries that bounce table can hold")
    bpq_penalty = Param.Latency("555ps", "Cycle penalty for bounce table access")

    wb_dest_reads = Param.Int(3, "Whether to writeback reads to destination; 0 = no, 1 = yes, 2 = adaptive")
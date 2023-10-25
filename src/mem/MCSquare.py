from m5.params import *
from m5.SimObject import SimObject

class MCSquare(SimObject):
    type = 'MCSquare'
    cxx_header = "mem/mcsquare.h"
    cxx_class = "gem5::memory::MCSquare"

    ctt_size = Param.Int(65536, "Number of entries that copy tracking table can hold")
    ctt_penalty = Param.Latency("5408ps", "Cycle penalty for reading a data copy that was elided")

    bpq_size = Param.Int(4, "Number of entries that bounce table can hold")
    bpq_penalty = Param.Latency("555ps", "Cycle penalty for bounce table access")

    wb_dest_reads = Param.Int(0, "Whether to writeback reads to destination; 0 = no, 1 = yes, 2 = adaptive")
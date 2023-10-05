from m5.params import *
from m5.SimObject import SimObject

class MCSquare(SimObject):
    type = 'MCSquare'
    cxx_header = "mem/mcsquare.h"
    cxx_class = "gem5::memory::MCSquare"

    ctt_size = Param.Int(65536, "Number of entries that copy tracking table can hold")
    ctt_penalty = Param.Latency("5408ps", "Cycle penalty for reading a data copy that was elided")

    bt_size = Param.Int(4, "Number of entries that bounce table can hold")
    bt_penalty = Param.Latency("555ps", "Cycle penalty for bounce table access")
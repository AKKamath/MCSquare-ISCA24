from m5.params import *
from m5.SimObject import SimObject

class MCSquare(SimObject):
    type = 'MCSquare'
    cxx_header = "mem/mcsquare.h"
    cxx_class = "gem5::memory::MCSquare"

    table_size = Param.Int(2048, "Number of entries that elision table can hold")
    table_penalty = Param.Latency("10ns", "Cycle penalty for reading a data copy that was elided")
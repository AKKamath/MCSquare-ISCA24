#ifndef MCSQUARE_H
#define MCSQUARE_H
#include <gem5/m5ops.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <x86intrin.h>
#include <string.h>
#include <algorithm>    // std::shuffle
#include <random>       // std::default_random_engine
#define SIZE (1024*4096)
#define PAGE_SIZE 4096
#define HUGE_PAGE_SIZE (1024l * 1024l * 2l)
#define PAGE_BITS 12
#define CL_SIZE 64
#define CL_BITS 6
#define ACCESSES (SIZE / sizeof(uint64_t))

#define cust_min(a, b) (((a) < (b)) ? (a) : (b))

#include <chrono>
using namespace std::chrono;
#define TIME_NOW high_resolution_clock::now()
#define TIME_DIFF(a, b) duration_cast<microseconds>(a - b).count()

#endif
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
#define SIZE (1024*4096)
#define PAGE_SIZE 4096
#define HUGE_PAGE_SIZE (1024l * 1024l * 2l)
#define PAGE_BITS 12
#define CL_SIZE 64
#define CL_BITS 6
#define ACCESSES (SIZE / sizeof(uint64_t))

#define cust_min(a, b) (((a) < (b)) ? (a) : (b))
#define MCLAZY(dest, src, size) \
        asm volatile(".byte 0x0F, 0x0A" : : "D"(dest), "S"(src), "d"(size));

#ifdef CHRONO
#include <chrono>
using namespace std::chrono;
#define TIME_NOW high_resolution_clock::now()
#define TIME_DIFF(a, b) duration_cast<microseconds>(a - b).count()
#endif

static inline uint64_t rdtsc(void)
{
    uint32_t eax, edx;
    asm volatile ("rdtsc" : "=a" (eax), "=d" (edx));
    return ((uint64_t) edx << 32) | eax;
}

static void *(*libc_memcpy)(void *dest, const void *src, size_t n) = memcpy;

static void memcpy_elide_clwb(void* dest, const void* src, uint64_t len)
{
    #ifdef TIME_MEMCPY
    time_elide = 0;
    uint64_t start = rdtsc();
    #endif
    uint64_t temp_src = ((uint64_t)src & ~((uint64_t)63));
    while(temp_src < (uint64_t)src + len) {
        _mm_clwb( (void*)temp_src );
        temp_src += CL_SIZE;
    }
    /*uint64_t temp_dest = ((uint64_t)dest & ~((uint64_t)4095));
    while(temp_dest < (uint64_t)dest + len) {
        _mm_clwb( (void*)temp_dest );
        temp_dest += PAGE_SIZE;
    }*/
    _mm_mfence();
    #ifdef TIME_MEMCPY
    time_flush = rdtsc() - start;
    start = rdtsc();
    #endif
    // Cacheline-align dest
    uint64_t left_fringe = CL_SIZE - ((uint64_t)dest & (CL_SIZE - 1));
    temp_src = (uint64_t)src;
    if(left_fringe < CL_SIZE) {
        libc_memcpy(dest, src, left_fringe);
        dest = (void *)((char *)dest + left_fringe);
        temp_src = ((uint64_t)src + left_fringe);
        len -= left_fringe;
    }
    #ifdef TIME_MEMCPY
    time_copy = rdtsc() - start;
    uint64_t start_total = rdtsc();
    #endif
    while(len > 0) {
        #ifdef TIME_MEMCPY
        start = rdtsc();
        #endif
        // Calculate remaining size in page for src and dest
        uint64_t src_off = PAGE_SIZE - ((uint64_t)temp_src & (PAGE_SIZE - 1));
        uint64_t dest_off = PAGE_SIZE - ((uint64_t)dest & (PAGE_SIZE - 1));
        // Pick minimum size left as elide_size
        uint64_t elide_size = cust_min(cust_min(src_off, dest_off), len);
        if(elide_size < CL_SIZE) {
          if(len >= CL_SIZE)
            elide_size = CL_SIZE;
          else
            elide_size = len;
          libc_memcpy(dest, (void*)temp_src, elide_size);
          #ifdef TIME_MEMCPY
          time_copy += rdtsc() - start;
          #endif
        }
        else {
          // Make elide size a multiple of 64
          elide_size &= (~63);
          MCLAZY(dest, (void*)temp_src, elide_size);
          #ifdef TIME_MEMCPY
          time_elide += rdtsc() - start;
          #endif
        }
        dest = (void *)((char *)dest + elide_size);
        temp_src = (temp_src + elide_size);
        len -= elide_size;
    }
    _mm_mfence();
    #ifdef TIME_MEMCPY
    time_lazy_total = rdtsc() - start_total;
    #endif
}

static void memcpy_elide_free(void* dest, uint64_t len)
{
    uint64_t temp_dest = ((uint64_t)dest & ~((uint64_t)63));
    while(temp_dest < (uint64_t)dest + len) {
        _mm_clwb( (void*)temp_dest );
        temp_dest += PAGE_SIZE;
    }
    while(len > 0) {
        // Calculate remaining size in page for dest
        uint64_t dest_off = PAGE_SIZE - ((uint64_t)dest & (PAGE_SIZE - 1));
        // Pick minimum size left as elide_size
        uint64_t free_size = cust_min(dest_off, len);
        m5_memcpy_elide_free(dest, free_size);
        dest = (void *)((char *)dest + free_size);
        len -= free_size;
    }
    _mm_mfence();
}
#endif
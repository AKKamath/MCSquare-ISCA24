/*
 * Copyright 2019 University of Washington, Max Planck Institute for
 * Software Systems, and The University of Texas at Austin
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <dlfcn.h>
#include <execinfo.h>
#include <poll.h>
#include <pthread.h>
#include <unistd.h>
#include "mcsquare.h"

#define OPT_THRESHOLD 255

#define UFFD_PROTO

#define LOGON 0
#if LOGON
#define LOG(...) fprintf(stderr, __VA_ARGS__)
#else
#define LOG(...)                                                               \
  while (0) {                                                                  \
  }
#endif

#define LOG_STATS(...) fprintf(stderr, __VA_ARGS__)

pthread_t fault_thread, stats_thread;

pthread_mutex_t mu;

static inline void ensure_init(void);

uint64_t num_fast_writes, num_slow_writes, num_fast_copy, num_slow_copy,
    num_faults;
uint64_t time_search, time_insert, time_other;

static void *(*libc_memcpy)(void *dest, const void *src, size_t n);
//static void (*libc_free)(void *ptr);

static inline uint64_t rdtsc(void)
{
    uint32_t eax, edx;
    asm volatile ("rdtsc" : "=a" (eax), "=d" (edx));
    return ((uint64_t) edx << 32) | eax;
}


static void memcpy_elide_clwb(void* dest, const void* src, uint64_t len)
{
    uint64_t temp_src = ((uint64_t)src & ~((uint64_t)63));
    while(temp_src < (uint64_t)src + len) {
        _mm_clwb( (void*)temp_src );
        temp_src += CL_SIZE;
    }
    uint64_t temp_dest = ((uint64_t)dest & ~((uint64_t)4095));
    while(temp_dest < (uint64_t)dest + len) {
        _mm_clwb( (void*)temp_dest );
        temp_dest += PAGE_SIZE;
    }
    _mm_mfence();
    // Cacheline-align dest
    uint64_t left_fringe = CL_SIZE - ((uint64_t)dest & (CL_SIZE - 1));
    temp_src = (uint64_t)src;
    if(left_fringe < CL_SIZE) {
        libc_memcpy(dest, src, left_fringe);
        dest = (void *)((char *)dest + left_fringe);
        temp_src = ((uint64_t)src + left_fringe);
    }
    while(len > 0) {
        // Calculate remaining size in page for src and dest
        uint64_t src_off = PAGE_SIZE - ((uint64_t)src & (PAGE_SIZE - 1));
        uint64_t dest_off = PAGE_SIZE - ((uint64_t)dest & (PAGE_SIZE - 1));
        // Pick minimum size left as elide_size
        uint64_t elide_size = cust_min(cust_min(src_off, dest_off), len);
        if(elide_size < CL_SIZE)
          memcpy(dest, (void*)temp_src, elide_size);
        else {
          // Make elide size a multiple of 64
          elide_size &= (~63);
          m5_memcpy_elide(dest, (void*)temp_src, elide_size);
        }
        dest = (void *)((char *)dest + elide_size);
        temp_src = ((uint64_t)src + elide_size);
        len -= elide_size;
    }
}

static void memcpy_elide_clwb_huge(void* dest, const void* src, uint64_t len)
{
    uint64_t temp_src = ((uint64_t)src & ~((uint64_t)63));
    while(temp_src < (uint64_t)src + len) {
        _mm_clwb( (void*)temp_src );
        temp_src += CL_SIZE;
    }
    _mm_mfence();
    // Cacheline-align dest
    uint64_t left_fringe = CL_SIZE - ((uint64_t)dest & (CL_SIZE - 1));
    temp_src = (uint64_t)src;
    if(left_fringe < CL_SIZE) {
        libc_memcpy(dest, src, left_fringe);
        dest = (void *)((char *)dest + left_fringe);
        temp_src = ((uint64_t)src + left_fringe);
    }
    while(len > 0) {
        // Calculate remaining size in page for src and dest
        uint64_t src_off = HUGE_PAGE_SIZE - ((uint64_t)src & (HUGE_PAGE_SIZE - 1));
        uint64_t dest_off = HUGE_PAGE_SIZE - ((uint64_t)dest & (HUGE_PAGE_SIZE - 1));
        // Pick minimum size left as elide_size
        uint64_t elide_size = cust_min(cust_min(src_off, dest_off), len);
        if(elide_size < CL_SIZE)
          memcpy(dest, (void*)temp_src, elide_size);
        else {
          // Make elide size a multiple of 64
          elide_size &= (~63);
          m5_memcpy_elide(dest, (void*)temp_src, elide_size);
        }
        dest = (void *)((char *)dest + elide_size);
        temp_src = ((uint64_t)src + elide_size);
        len -= elide_size;
    }
}

static void memcpy_elide_free(void* dest, uint64_t len)
{
    m5_memcpy_elide_free(dest, 1);
    _mm_mfence();
}

void print_trace(void) {
  char **strings;
  size_t i, size;
  enum Constexpr { MAX_SIZE = 1024 };
  void *array[MAX_SIZE];
  size = backtrace(array, MAX_SIZE);
  strings = backtrace_symbols(array, size);
  for (i = 0; i < 5; i++)
    LOG("%s\n", strings[i]);
  free(strings);
}

void *memcpy(void *dest, const void *src, size_t n) {
  ensure_init();
  const char cannot_optimize = (n <= OPT_THRESHOLD);

  if (cannot_optimize) {
    //printf("Cannot elide, memcpying\n");
    return libc_memcpy(dest, src, n);
  }

  static int ignore = 2;
  if(ignore) {
    --ignore;
    if(ignore == 0) {
      printf("Starting memcpy\n");
    }
    return libc_memcpy(dest, src, n);
  }

  //printf("Eliding\n");
  memcpy_elide_clwb(dest, src, n);
  return dest;
}

//void free(void *ptr) {
  // uint64_t ptr_bounded = (uint64_t)ptr & PAGE_MASK;
  // snode *entry = skiplist_search(&addr_list, ptr_bounded);

  // if (entry) {
  //   if (entry->orig == ptr) {
  //     // mark for later free
  //     entry->free = 1;
  //     return;
  //   } else {
  //     skiplist_delete(&addr_list, ptr_bounded);
  //   }
  // }
  //return libc_free(ptr);
//}


/******************************************************************************/
/* Helper functions */

static void *bind_symbol(const char *sym) {
  void *ptr;
  if ((ptr = dlsym(RTLD_NEXT, sym)) == NULL) {
    fprintf(stderr, "flextcp socket interpose: dlsym failed (%s)\n", sym);
    abort();
  }
  return ptr;
}

void *print_stats() {
  while (1) {
    LOG_STATS("fast copies: %lu\tslow copies: %lu\tfast writes: %lu\tslow "
              "writes: %lu\tpage faults: %lu\n",
              num_fast_copy, num_slow_copy, num_fast_writes, num_slow_writes,
              num_faults);
    
    double total_time = time_search + time_insert + time_other;
    LOG_STATS("Time: search = %lu (%.2f%%), insert =  %lu (%.2f%%), other =  %lu (%.2f%%)\n",
              time_search, (double)time_search / total_time * 100.0, 
              time_insert, (double)time_insert / total_time * 100.0, 
              time_other, (double)time_other / total_time * 100.0);
    num_fast_writes = num_slow_writes = num_fast_copy = num_slow_copy =
        num_faults = 0;
    time_search = time_insert = time_other = 0;
    sleep(1);
  }
}

static void init(void) {
  printf("MCSquare start\n");

  libc_memcpy = (void* (*)(void*, const void*, long unsigned int))bind_symbol("memcpy");
  //libc_free = bind_symbol("free");
  
  pthread_mutex_init(&mu, NULL);

#ifdef UFFD_PROTO
  num_fast_writes = num_slow_writes = num_fast_copy = num_slow_copy =
      num_faults = 0;
#endif
}

static inline void ensure_init(void) {
  static volatile uint32_t init_cnt = 0;
  static volatile uint8_t init_done = 0;
  static __thread uint8_t in_init = 0;

  if (init_done == 0) {
    /* during init the socket functions will be used to connect to the kernel
     * on a unix socket, so make sure that runs through. */
    if (in_init) {
      return;
    }

    if (__sync_fetch_and_add(&init_cnt, 1) == 0) {
      in_init = 1;
      init();
      in_init = 0;
      init_done = 1;
    } else {
      while (init_done == 0) {
        sched_yield();
      }
    }
  }
}

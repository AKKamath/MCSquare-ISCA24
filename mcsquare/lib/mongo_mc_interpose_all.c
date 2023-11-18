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
#include <asm-generic/errno-base.h>
#include <bits/types/struct_iovec.h>
#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define __USE_GNU
#include <assert.h>
#include <dlfcn.h>
#include <execinfo.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <skiplist.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

#define TIME_MEMCPY
uint64_t time_flush = 0, time_elide = 0, time_copy = 0, time_lazy_total = 0;
#include "mcsquare.h"
#define MEM_BARRIER() __asm__ volatile("" ::: "memory")

#ifndef OPT_THRESHOLD
#define OPT_THRESHOLD 64000
#endif

#define LOG_STATS(...) fprintf(stderr, __VA_ARGS__)

#define MIN(x, y) ((x) < (y) ? (x) : (y))
uint64_t time_search = 0, time_insert = 0, time_other = 0;

static inline void ensure_init(void);

uint64_t num_fast_writes, num_slow_writes, num_fast_copy, num_slow_copy,
    num_faults;

static void *(*libc_memcpy)(void *dest, const void *src, size_t n);
static ssize_t (*libc_recvmsg)(int sockfd, struct msghdr *msg, int flags);

int started = 0;

ssize_t recvmsg(int sockfd, struct msghdr *msg, int flags) {
  ensure_init();

  started = 1;

  return libc_recvmsg(sockfd, msg, flags);
}

void *memcpy(void *dest, const void *src, size_t n) {
  ensure_init();

  const char cannot_optimize = (n <= OPT_THRESHOLD);

  if (cannot_optimize || !started) {
    return libc_memcpy(dest, src, n);
  }

  memcpy_elide_clwb(dest, src, n);

  // can be partial copy
  ++num_fast_copy;
  //fprintf(stderr, "Copies %lu time: search_total %lu, insert %lu, flush %lu, "
  //        "copy %lu, elide %lu, copy_total %lu\n", num_fast_copy, time_search, 
  //        time_insert, time_flush, time_copy, time_elide, time_lazy_total);
  return dest;
}

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

static void init(void) {
  fprintf(stdout, "MCSquare start\n");
  libc_memcpy = bind_symbol("memcpy");
  libc_recvmsg = bind_symbol("recvmsg");
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
      MEM_BARRIER();
      init_done = 1;
    } else {
      while (init_done == 0) {
        pthread_yield();
      }
      MEM_BARRIER();
    }
  }
}

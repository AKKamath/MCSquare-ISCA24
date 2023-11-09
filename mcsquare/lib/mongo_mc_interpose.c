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
#include "mcsquare.h"
#define MEM_BARRIER() __asm__ volatile("" ::: "memory")

#define OPT_THRESHOLD 2048

#define LOGON 0
#if LOGON
#define LOG(...) fprintf(stderr, __VA_ARGS__)
#else
#define LOG(...)                                                               \
  while (0) {                                                                  \
  }
#endif

#define LOG_STATS(...) fprintf(stderr, __VA_ARGS__)

#define print(addr1, addr2, len)                                               \
  do {                                                                         \
    const int is_only_addr1 = (addr1 && !addr2);                               \
    const int is_only_addr2 = (!addr1 && addr2);                               \
    if (is_only_addr1) {                                                       \
      fprintf(stdout, "%s len:%zu %p(%lu)\n", __func__, len, addr1,            \
              (uint64_t)addr1 &PAGE_MASK);                                     \
    } else if (is_only_addr2) {                                                \
      fprintf(stdout, "%s len:%zu %p(%lu)\n", __func__, len, addr2,            \
              (uint64_t)addr2 &PAGE_MASK);                                     \
    } else {                                                                   \
      fprintf(stdout, "%s len:%zu %p(%lu)->%p(%lu)\n", __func__, len, addr1,   \
              (uint64_t)addr1 &PAGE_MASK, addr2, (uint64_t)addr2 &PAGE_MASK);  \
    }                                                                          \
  } while (0)

#define MIN(x, y) ((x) < (y) ? (x) : (y))

#define IOV_MAX_CNT 10000

#if ENABLED_LOCK
pthread_mutex_t mu;
#endif

static inline void ensure_init(void);

uint64_t num_fast_writes, num_slow_writes, num_fast_copy, num_slow_copy,
    num_faults;

static void *(*libc_memcpy)(void *dest, const void *src, size_t n);
static void *(*libc_memmove)(void *dest, const void *src, size_t n);
static ssize_t (*libc_pwrite)(int fd, const void *buf, size_t count,
                              off_t offset) = NULL;
static ssize_t (*libc_pwritev)(int sockfd, const struct iovec *iov, int iovcnt,
                               off_t offset) = NULL;
static void (*libc_free)(void *ptr);
static ssize_t (*libc_send)(int sockfd, const void *buf, size_t count,
                            int flags);
static ssize_t (*libc_sendmsg)(int sockfd, const struct msghdr *msg, int flags);

static ssize_t (*libc_recv)(int sockfd, void *buf, size_t len, int flags);
static ssize_t (*libc_recvmsg)(int sockfd, struct msghdr *msg, int flags);

skiplist addr_list;

void print_trace(void) {
  char **strings;
  size_t i, size;
  enum Constexpr { MAX_SIZE = 1024 };
  void *array[MAX_SIZE];
  size = backtrace(array, MAX_SIZE);
  strings = backtrace_symbols(array, size);
  for (i = 0; i < 15; i++)
    printf("%s\n", strings[i]);
  /* if (strings) */
  /*   libc_free(strings); */
}

void *dest_in_processing = 0;

void *memcpy(void *dest, const void *src, size_t n) {
  ensure_init();

  static uint64_t prev_start, prev_end;
  // TODO: parse big copy for multiple small copies

  const char cannot_optimize = (n <= OPT_THRESHOLD);

  if (cannot_optimize) {
    dest_in_processing = 0;
    return libc_memcpy(dest, src, n);
  }

  //LOG("[%s] copying %p-%p to %p-%p, size %zu\n", __func__, src, src + n, dest,
  //    dest + n, n);

  //uint64_t left_fringe = ((uint64_t)src & 4095);
  //if(left_fringe > 0) 
  //  left_fringe = 4096 - left_fringe;
  const uint64_t core_src_buffer_addr = src;// + left_fringe;

  if (dest_in_processing == 0)
    dest_in_processing = dest;

#if ENABLED_LOCK
  pthread_mutex_lock(&mu);
#endif

  snode *src_entry = skiplist_search_buffer_fallin(&addr_list, core_src_buffer_addr);
  if (src_entry) {
#if LOGON
    LOG("[%s] found src entry\n", __func__);
    snode_dump(src_entry);
#endif

    uint64_t core_dst_buffer_addr = dest;

    snode dest_entry;
    dest_entry.lookup = core_dst_buffer_addr;
    dest_entry.orig =
        src_entry->orig + ((long long)src - (long long)src_entry->addr);
    dest_entry.addr = dest;
    dest_entry.len = n;
    dest_entry.offset = 0;

    size_t remaining_len = n;

    if (dest_entry.len > OPT_THRESHOLD) {
      //if (PAGE_ALIGN_DOWN(dest_entry.addr) !=
      //    PAGE_ALIGN_DOWN(dest_entry.orig)) {
        skiplist_insert_entry(&addr_list, &dest_entry);
        memcpy_elide_clwb(dest, src, n);

        LOG("[%s] tracking buffer %p-%p len:%lu\n", __func__,
            dest_entry.addr + dest_entry.offset,
            dest_entry.addr + dest_entry.offset + dest_entry.len,
            dest_entry.len);
#if LOGON
        snode_dump(&dest_entry);
#endif

      /*} else {
        if (dest_entry.addr != dest_entry.orig) {
          libc_memmove(dest_entry.addr + dest_entry.offset,
                       dest_entry.orig + dest_entry.offset, dest_entry.len);
          LOG("[%s] copy buffer %p-%p -> %p-%p len:%lu\n", __func__,
              dest_entry.orig + dest_entry.offset,
              dest_entry.orig + dest_entry.offset + dest_entry.len,
              dest_entry.addr + dest_entry.offset,
              dest_entry.addr + dest_entry.offset + dest_entry.len,
              dest_entry.len);
        }
      }*/
    }

    LOG("[%s] remaining_len %zu out of %zu\n", __func__, remaining_len, n);

    // can be partial copy
    ++num_fast_copy;

#if ENABLED_LOCK
    pthread_mutex_unlock(&mu);
#endif
  } else {
    // can be partial copy
    ++num_slow_copy;

#if ENABLED_LOCK
    pthread_mutex_unlock(&mu);
#endif
    libc_memcpy(dest, src, n);
  }

  return dest;
}

void free(void *ptr) {
  ensure_init();

  /*
  snode *entry = skiplist_front(&addr_list);
  while (entry) {
    if (entry->addr == ptr) {
      UNREGISTER_FAULT(entry->addr + entry->offset, entry->len);
      skiplist_delete(&addr_list, entry->lookup);
      break;
    }

    entry = snode_get_next(&addr_list, entry);
  }
  */
  return libc_free(ptr);
}

/*ssize_t sendmsg(int sockfd, const struct msghdr *msg, int flags) {
  ensure_init();

  struct iovec iovec[IOV_MAX_CNT];
  int iovcnt = 0;

  int i;
  for (i = 0; i < msg->msg_iovlen; ++i) {
    void *buf = msg->msg_iov[i].iov_base;
    uint64_t count = msg->msg_iov[i].iov_len;

    LOG("[%s] base: %p, count: %zu\n", __func__, buf, count);

    if (count > OPT_THRESHOLD) {
#if ENABLED_LOCK
      pthread_mutex_lock(&mu);
#endif

      uint64_t off = 0;
      uint64_t remaining_len = count;

      while (remaining_len > OPT_THRESHOLD) {
        snode *entry =
            skiplist_search_buffer_fallin(&addr_list, (uint64_t)buf + off);

        if (entry) {
          iovec[iovcnt].iov_base = entry->orig + (buf - entry->addr) + off;
          iovec[iovcnt].iov_len = entry->len;
        } else {
          iovec[iovcnt].iov_base = buf + off;
          iovec[iovcnt].iov_len = LEFT_FRINGE_LEN(iovec[iovcnt].iov_base) == 0
                                      ? PAGE_SIZE
                                      : LEFT_FRINGE_LEN(iovec[iovcnt].iov_base);
        }

        iovec[iovcnt].iov_len = MIN(remaining_len, iovec[iovcnt].iov_len);

        off += iovec[iovcnt].iov_len;
        remaining_len -= iovec[iovcnt].iov_len;

        ++iovcnt;

        if (iovcnt >= IOV_MAX_CNT) {
          errno = ENOMEM;
          perror("iov is full");
          abort();
        }
      }

      if (remaining_len > 0) {
        iovec[iovcnt].iov_base = buf + off;
        iovec[iovcnt].iov_len = remaining_len;
        ++iovcnt;
      }

#if ENABLED_LOCK
      pthread_mutex_unlock(&mu);
#endif
#if LOGON
      {
        int i;
        int total_len = 0;
        for (i = 0; i < iovcnt; i++) {
          printf("iov[%d]: base %p len %lu\n", i, iovec[i].iov_base,
                 iovec[i].iov_len);
          total_len += iovec[i].iov_len;
        }

        printf("total: %d, count: %d\n", total_len, count);
      }
#endif
    } else {
      iovec[iovcnt].iov_base = buf;
      iovec[iovcnt].iov_len = count;
      ++iovcnt;
    }
  }

  struct msghdr mh;
  libc_memcpy(&mh, msg, sizeof(struct msghdr));
  mh.msg_iov = iovec;
  mh.msg_iovlen = iovcnt;

  return libc_sendmsg(sockfd, &mh, flags);
}*/

ssize_t recvmsg(int sockfd, struct msghdr *msg, int flags) {
  ensure_init();

  ssize_t ret = libc_recvmsg(sockfd, msg, flags);

  int i;
  for (i = 0; i < msg->msg_iovlen; i++) {
    ssize_t core_buffer_len = msg->msg_iov[i].iov_len;

    LOG("[%s] i: %d, iov_base: %p, iov_len: %zu\n", __func__, i,
        msg->msg_iov[i].iov_base, msg->msg_iov[i].iov_len);
    //uint64_t left_fringe = ((uint64_t)msg->msg_iov[i].iov_base & 4095);
    //if(left_fringe > 0) 
    //  left_fringe = 4096 - left_fringe;
    if (core_buffer_len > OPT_THRESHOLD) {
      snode new_entry;
      new_entry.lookup = msg->msg_iov[i].iov_base;// + left_fringe;
      new_entry.orig = (uint64_t)msg->msg_iov[i].iov_base;
      new_entry.addr = (uint64_t)msg->msg_iov[i].iov_base;
      new_entry.len = core_buffer_len;
      new_entry.offset = 0;

#if ENABLED_LOCK
      pthread_mutex_lock(&mu);
#endif

      skiplist_insert_entry(&addr_list, &new_entry);

#if ENABLED_LOCK
      pthread_mutex_unlock(&mu);
#endif
    }
  }

  return ret;
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

void *print_stats() {
  while (1) {
    LOG_STATS("fast copies: %lu\tslow copies: %lu\tfast writes: %lu\tslow "
              "writes: %lu\tpage faults: %lu\n",
              num_fast_copy, num_slow_copy, num_fast_writes, num_slow_writes,
              num_faults);
    num_fast_writes = num_slow_writes = num_fast_copy = num_slow_copy =
        num_faults = 0;
    sleep(1);
  }
}

static void init(void) {
  fprintf(stdout, "zIO start\n");

  libc_pwrite = bind_symbol("pwrite");
  libc_pwritev = bind_symbol("pwritev");
  libc_memcpy = bind_symbol("memcpy");
  libc_memmove = bind_symbol("memmove");
  libc_free = bind_symbol("free");
  libc_send = bind_symbol("send");
  libc_sendmsg = bind_symbol("sendmsg");
  libc_recv = bind_symbol("recv");
  libc_recvmsg = bind_symbol("recvmsg");

  // new tracking code
  skiplist_init(&addr_list);
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

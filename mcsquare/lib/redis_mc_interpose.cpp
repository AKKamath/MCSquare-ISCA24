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

//#define OPT_THRESHOLD 0xfffffffffffffffff
// #define OPT_THRESHOLD 1048575
#define OPT_THRESHOLD 1023

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

#define IOV_MAX_CNT 10000

long uffd = -1;

pthread_mutex_t mu;

static inline void ensure_init(void);

uint64_t num_fast_writes, num_slow_writes, num_fast_copy, num_slow_copy,
    num_faults;
uint64_t time_search, time_insert, time_other;

static void *(*libc_memmove)(void *dest, const void *src, size_t n);
static ssize_t (*libc_pwrite)(int fd, const void *buf, size_t count,
                              off_t offset) = NULL;
static ssize_t (*libc_pwritev)(int sockfd, const struct iovec *iov, int iovcnt,
                               off_t offset) = NULL;
static void *(*libc_realloc)(void *ptr, size_t new_size);
//static void (*libc_free)(void *ptr);
static ssize_t (*libc_send)(int sockfd, const void *buf, size_t count,
                            int flags);
static ssize_t (*libc_sendmsg)(int sockfd, const struct msghdr *msg, int flags);

static ssize_t (*libc_recv)(int sockfd, void *buf, size_t len, int flags);
static ssize_t (*libc_recvmsg)(int sockfd, struct msghdr *msg, int flags);
static ssize_t (*libc_read)(int fd, void *buf, size_t count) = NULL;

skiplist addr_list;

static inline uint64_t rdtsc(void)
{
    uint32_t eax, edx;
    asm volatile ("rdtsc" : "=a" (eax), "=d" (edx));
    return ((uint64_t) edx << 32) | eax;
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

int recursive_copy = 0;

void *memcpy(void *dest, const void *src, size_t n) {
  ensure_init();

  uint64_t start;
  // TODO: parse big copy for multiple small copies

  const char cannot_optimize = (n <= OPT_THRESHOLD);

  if (cannot_optimize) {
    return libc_memcpy(dest, src, n);
  }

  if (recursive_copy == 0)
    pthread_mutex_lock(&mu);

#if LOGON
  printf("[%s] copying %p-%p to %p-%p, size %zu\n", __func__, src, src + n,
         dest, dest + n, n);
#endif

  const uint64_t core_src_buffer_addr = (uint64_t)src;
  uint64_t core_dst_buffer_addr = (uint64_t)dest;

  //if (recursive_copy == 0)
    //handle_existing_buffer(core_dst_buffer_addr);
  start = rdtsc();
  snode *src_entry =
      skiplist_search_buffer_fallin(&addr_list, core_src_buffer_addr);
  
  time_search += rdtsc() - start;

  if (src_entry) {
#if LOGON
    printf("[%s] found src entry\n", __func__);
    snode_dump(src_entry);
#endif
    start = rdtsc();

    core_dst_buffer_addr = (uint64_t)dest;

    snode dest_entry;
    dest_entry.lookup = core_dst_buffer_addr;
    dest_entry.orig =
        src_entry->orig + ((long long)src - (long long)src_entry->addr);
    dest_entry.addr = (uint64_t)dest;
    dest_entry.len = n;
    dest_entry.offset = 0;

    time_other += rdtsc() - start;

    if (dest_entry.len > OPT_THRESHOLD) {
      start = rdtsc();
      skiplist_insert_entry(&addr_list, &dest_entry);
      memcpy_elide_clwb(dest, src, n);
      LOG("[%s] tracking buffer %p-%p len:%lu\n", __func__,
             dest_entry.addr + dest_entry.offset,
             dest_entry.addr + dest_entry.offset + dest_entry.len,
             dest_entry.len);
#if LOGON
      snode_dump(&dest_entry);
#endif
      time_insert += rdtsc() - start;
    }
    start = rdtsc();
    LOG("[%s] remaining_len %zu out of %zu\n", __func__, remaining_len, n);
    ++num_fast_copy;

    LOG("[%s] ########## Fast copy done\n", __func__);
    pthread_mutex_unlock(&mu);
    time_other += rdtsc() - start;
    return dest;
  } else {
    if (recursive_copy == 0) {
      ++num_slow_copy;

      LOG("[%s] ########## Slow copy done\n", __func__);
      pthread_mutex_unlock(&mu);
    }

    return libc_memcpy(dest, src, n);
  }
}

//void free(void *ptr) {
  // uint64_t ptr_bounded = (uint64_t)ptr & PAGE_MASK;
  // snode *entry = skiplist_search_buffer_fallin(&addr_list, ptr_bounded);

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

ssize_t send(int sockfd, const void* buf, size_t count, int flags) {
  ensure_init();

  pthread_mutex_lock(&mu);

  ssize_t ret = 0;

  //int i;

  if (count > OPT_THRESHOLD) {

    uint64_t off = 0;
    //uint64_t remaining_len = count;
    uint64_t send_buf, send_len;

    snode *entry =
        skiplist_search_buffer_fallin(&addr_list, (uint64_t)buf + off);

    if (entry) {
      //iovec[iovcnt].iov_base = entry->orig + (buf - entry->addr) + off;
      send_buf = entry->orig + ((uint64_t)buf - entry->addr) + off;
      //iovec[iovcnt].iov_len = entry->len;
      send_len = entry->len;
    } else {
      send_buf = (uint64_t)buf;
      send_len = count;
    }

    ssize_t sent = libc_send(sockfd, (void *)send_buf, send_len, flags);
    if (sent < 0) {
      perror("send error");
      abort();
    }
    ret += sent;
  } else {
    ssize_t sent = libc_send(sockfd, buf, count, flags);
    if (sent < 0) {
      perror("send error");
      abort();
    }
    ret += sent;
  }

  pthread_mutex_unlock(&mu);
  return ret;
}

ssize_t recv(int sockfd, void* buf, size_t count, int flags) {
  ensure_init();

  ssize_t ret = 0;
  if(sockfd != -2)
    ret = libc_recv(sockfd, buf, count, flags);
  uint64_t buf_addr = (uint64_t) buf;

  pthread_mutex_lock(&mu);

  //int i;

  uint64_t core_buffer_len = count;
  if (count > OPT_THRESHOLD ) { //&& core_buffer_len > OPT_THRESHOLD) {
    snode new_entry;
    new_entry.lookup = buf_addr;
    new_entry.orig = buf_addr;
    new_entry.addr = buf_addr;
    new_entry.len = core_buffer_len;
    new_entry.offset = 0;

    //handle_existing_buffer(new_entry.lookup);

    snode *prev = skiplist_search_buffer_fallin(
        &addr_list, new_entry.addr - 1);

    if (0 && prev &&
        prev->addr + prev->offset + prev->len + PAGE_SIZE ==
            new_entry.addr + new_entry.offset) {
      LOG("[%s] %p will be merged to %p-%p, %lu\n", __func__,
              new_entry.addr, prev->addr, prev->addr + prev->len, prev->len);

      prev->len += new_entry.len + (new_entry.offset == 0 ? 0 : PAGE_SIZE);
    } else {
      skiplist_insert_entry(&addr_list, &new_entry);
    }
  }

  pthread_mutex_unlock(&mu);
  return ret;
}


ssize_t read(int sockfd, void *buf, size_t count)
{
  ssize_t ret = 0;
  static void* prev_addr, *prev_orig; //, *max_addr;
  static size_t prev_len;
  uint64_t original;
  ensure_init();

  //fprintf(stderr, "tas read %zu bytes, page mask %lx, socket %d\n", ret, ((uint64_t) buf), sockfd);
  if(count > OPT_THRESHOLD){
    //if(ret > OPT_THRESHOLD){
    LOG("receiving data\n");
    ret = libc_read(sockfd, buf, count);
    if (ret == -1){
          perror("linux read");
      return ret;
    } 
    //if((uint64_t) original > (uint64_t) max_addr) max_addr = original;

    //uint64_t original = tas_get_buf_addr(sockfd, buf); 
    
    uint64_t core_buffer_len = count;
    snode new_entry;
    new_entry.lookup = (uint64_t)buf;
    new_entry.orig = (uint64_t)buf;
    new_entry.addr = (uint64_t)buf;
    new_entry.len = core_buffer_len;
    new_entry.offset = 0;
    skiplist_insert_entry(&addr_list, &new_entry);
    LOG("got and inserted %zu out of %zu\n", ret, count);
    
    return ret;
  } else {
    return libc_read(sockfd, buf, count);
  }
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
  //while (1) {
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
    //sleep(1);
  //}
  return NULL;
}

static void init(void) {
  printf("MCSquare start\n");

  libc_pwrite = (ssize_t(*)(int, const void *, size_t, off_t))
                bind_symbol("pwrite");
  libc_pwritev = (ssize_t(*)(int, const struct iovec *, int, off_t))
                 bind_symbol("pwritev");
  libc_memcpy = (void *(*)(void *, const void *, size_t))bind_symbol("memcpy");
  libc_memmove = (void *(*)(void *, const void *, size_t))bind_symbol("memmove");
  libc_realloc = (void *(*)(void *, size_t))bind_symbol("realloc");
  //libc_free = bind_symbol("free");
  
  
  libc_send = (ssize_t (*)(int, const void *, size_t, int))bind_symbol("send");
  libc_sendmsg = (ssize_t (*)(int, const struct msghdr *, int))bind_symbol("sendmsg");
  libc_recv = (ssize_t (*)(int, void *, size_t, int))bind_symbol("recv");
  libc_recvmsg = (ssize_t (*)(int, struct msghdr *, int))bind_symbol("recvmsg");
  libc_read = (ssize_t (*)(int, void *, size_t))bind_symbol("read");

  // new tracking code
  skiplist_init(&addr_list);

  pthread_mutex_init(&mu, NULL);

  num_fast_writes = num_slow_writes = num_fast_copy = num_slow_copy =
      num_faults = 0;
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
        sched_yield();
      }
      MEM_BARRIER();
    }
  }
}

struct setup_handler {
  ~setup_handler() {
    print_stats();
  }
} dummy;

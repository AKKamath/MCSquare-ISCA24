/*
 * Copyright 2023 University of Washington
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
#include <unordered_map>
#include <malloc.h>
#include "mcsquare.h"

#define OPT_THRESHOLD 1024

static void *(*libc_malloc)(size_t size) = NULL;
static ssize_t (*libc_recv)(int sockfd, void *buf, size_t len, int flags) = NULL;
static void (*libc_free)(void *ptr) = NULL;

bool init_done = false;
uint64_t elisions = 0;
bool start = false;

/********************************/
/*      Helper functions        */
/********************************/

static void *bind_symbol(const char *sym) {
  void *ptr;
  if ((ptr = dlsym(RTLD_NEXT, sym)) == NULL) {
    fprintf(stderr, "flextcp socket interpose: dlsym failed (%s)\n", sym);
    abort();
  }
  return ptr;
}

static void init(void) {
  fprintf(stderr, "MCSquare start\n");

  libc_memcpy = (void* (*)(void*, const void*, long unsigned int))bind_symbol("memcpy");
  libc_recv = (ssize_t (*)(int, void *, size_t, int))bind_symbol("recv");
  //libc_malloc = (void* (*)(size_t))bind_symbol("malloc");
  libc_free   = (void  (*)(void *))bind_symbol("free");
  //libc_munmap = (int   (*)(void *addr, size_t length))bind_symbol("munmap");

  //fprintf(stderr, "Memcpy %p, malloc %p\n", libc_memcpy, libc_malloc);
  init_done = true;
}

/********************************/
/*     Interposed functions     */
/********************************/

void *memcpy(void *dest, const void *src, size_t n) {
  if(!init_done)
    init();

  if ((n <= OPT_THRESHOLD)) {
    return libc_memcpy(dest, src, n);
  }

  if(!start) {
    return libc_memcpy(dest, src, n);
  }
  ++elisions;
  memcpy_elide_clwb(dest, src, n);
  //return libc_memcpy(dest, src, n);
  return dest;
}

ssize_t recv(int sockfd, void* buf, size_t count, int flags) {
  if(!init_done)
    init();

  if(sockfd == -5 && count == 0) {
    start = true;
    return libc_recv(sockfd, buf, count, flags);
  }

  if(sockfd == -6 && count == 0) {
    start = false;
    return libc_recv(sockfd, buf, count, flags);
  }

  return libc_recv(sockfd, buf, count, flags);;
}

/*
void free(void *ptr) {
  if(!init_done)
    init();
  size_t size = malloc_usable_size(ptr);
  
  if(size <= OPT_THRESHOLD)
    return libc_free(ptr);

  //fprintf(stderr, "freeing %p of %ld size; ", ptr, size);
  //memcpy_elide_free(ptr, size);
  //fprintf(stderr, "Freed %p\n", ptr);
  return libc_free(ptr);
}*/

struct setup_handler {
  ~setup_handler() {
    fprintf(stderr, "Total elisions: %lu\n", elisions);
    memcpy_elide_free(this, 1);
  }
} dummy;
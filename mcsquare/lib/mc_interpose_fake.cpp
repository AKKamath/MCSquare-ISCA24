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
#include <unordered_map>
#include "mcsquare.h"

#define OPT_THRESHOLD 1023

static void *(*libc_memcpy)(void *dest, const void *src, size_t n);
//static void *(*libc_malloc)(size_t size) = NULL;
//static void (*libc_free)(void *ptr) = NULL;

bool init_done = false;

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
  fprintf(stderr, "MCSquare start\n");

  libc_memcpy = (void* (*)(void*, const void*, long unsigned int))bind_symbol("memcpy");
  //libc_malloc = (void* (*)(size_t))bind_symbol("malloc");
  //libc_free   = (void  (*)(void *))bind_symbol("free");
  //libc_munmap = (int   (*)(void *addr, size_t length))bind_symbol("munmap");;

  //fprintf(stderr, "Memcpy %p, malloc %p\n", libc_memcpy, libc_malloc);
  init_done = true;
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
        len -= left_fringe;
    }
    while(len > 0) {
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
        }
        else {
          // Make elide size a multiple of 64
          elide_size &= (~63);
          //fprintf(stderr, "Elide %p 0x%lx %lu\n", dest, temp_src, elide_size);
          libc_memcpy(dest, (void*)temp_src, elide_size);
          //m5_memcpy_elide(dest, (void*)temp_src, elide_size);
        }
        dest = (void *)((char *)dest + elide_size);
        temp_src = (temp_src + elide_size);
        len -= elide_size;
    }
    _mm_mfence();
}

static void memcpy_elide_free(void* dest, uint64_t len)
{
    uint64_t temp_dest = ((uint64_t)dest & ~((uint64_t)63));
    while(temp_dest < (uint64_t)dest + len) {
        _mm_clwb( (void*)temp_dest );
        temp_dest += PAGE_SIZE;
    }
    _mm_mfence();
    while(len > 0) {
        // Calculate remaining size in page for dest
        uint64_t dest_off = PAGE_SIZE - ((uint64_t)dest & (PAGE_SIZE - 1));
        // Pick minimum size left as elide_size
        uint64_t free_size = cust_min(dest_off, len);
        //m5_memcpy_elide_free(dest, free_size);
        fprintf(stderr, "Freed %p (%lu)\n", dest, free_size);
        dest = (void *)((char *)dest + free_size);
        len -= free_size;
    }
    _mm_mfence();
}

void *memcpy(void *dest, const void *src, size_t n) {
  if(!init_done)
    init();
  if ((n <= OPT_THRESHOLD)) {
    return libc_memcpy(dest, src, n);
  }

  static int ignore = 2;
  if(ignore) {
    --ignore;
    if(ignore == 0) {
      fprintf(stderr, "Starting memcpy\n");
    }
    return libc_memcpy(dest, src, n);
  }
  memcpy_elide_clwb(dest, src, n);
  return dest;
} 
/*
void* operator new(std::size_t count) {
  fprintf(stderr, "Called new1\n");
}
void* operator new(std::size_t count, const std::nothrow_t& tag) noexcept {
  fprintf(stderr, "Called new2\n");
}
void* operator new(std::size_t count, std::align_val_t al) {
  fprintf(stderr, "Called new3\n");
}  // C++17
void* operator new(std::size_t count,
                   std::align_val_t al, const std::nothrow_t&) noexcept {
  fprintf(stderr, "Called new4\n");
}  // C++17

void *malloc(size_t size) {
  if(!init_done)
    init();
  fprintf(stderr, "Malloc called size %ld; ", size);
  return libc_malloc(size);
}

void free(void *ptr) {
  if(!init_done)
    init();
  fprintf(stderr, "Called free\n");
  return libc_free(ptr);
}*/


struct setup_handler {
  ~setup_handler() {
    memcpy_elide_free(this, 1);
  }
} dummy;
cd /home/akkamath/gem5-zIO/util/m5
scons build/x86/out
cd /home/akkamath/gem5-zIO/mcsquare
ls
echo "
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
#define PAGE_BITS 12
#define CL_SIZE 64
#define CL_BITS 6
#define ACCESSES (SIZE / sizeof(uint64_t))

#define cust_min(a, b) (((a) < (b)) ? (a) : (b))

#include <chrono>
using namespace std::chrono;
#define TIME_NOW high_resolution_clock::now()
#define TIME_DIFF(a, b) duration_cast<microseconds>(a - b).count()

static uint64_t lfsr_fast(uint64_t lfsr)
{
  lfsr ^= lfsr >> 7;
  lfsr ^= lfsr << 9;
  lfsr ^= lfsr >> 13;
  return lfsr;
}

#define TEST_OP(OPERATION, dest, src, size, accesses) \
    reset_op(dest, src, size); \
    _mm_mfence();  \
    printf(\"Dest: %lu Src: %lu\n\", *dest, *src); \
    _mm_mfence(); \
    m5_reset_stats(0, 0); \
    OPERATION(dest, src, size); \
    random_test(dest, src, size, accesses); \
    _mm_mfence(); \
    m5_dump_stats(0, 0); \
    memcpy_elide_free(dest, size);

void init_op(uint64_t* dest, uint64_t* src, uint64_t size) {
    // Set initial values
    for(int i = 0; i < size / sizeof(uint64_t); i++) {
        src[i] = i;
        dest[i] = 0;
    }

    // use a fixed seed:
    unsigned seed = 100;
    // Shuffle src into a random walk
    std::shuffle (src, src + size / sizeof(uint64_t), std::default_random_engine(seed));
}

void reset_op(uint64_t* dest, uint64_t* src, uint64_t size) {
    memset(dest, 0, size);
}

void random_test(uint64_t* dest, uint64_t* src, uint64_t size, uint64_t accesses) {
    uint64_t index = 0;
    for(uint64_t i = 0; i < accesses; i++) {
        index = dest[index];
    }
    printf(\"Verify: %lu\n\", index);
}

void memcpy_elide_pgflush(void* dest, void* src, uint64_t len)
{
    uint64_t temp_src = ((uint64_t)src & ~((uint64_t)63));
    while(temp_src < (uint64_t)src + len) {
        _mm_clwb( (void*)temp_src );
        temp_src += PAGE_SIZE;
    }
    _mm_mfence();
    // Cacheline-align dest
    uint64_t left_fringe = CL_SIZE - ((uint64_t)dest & (CL_SIZE - 1));
    if(left_fringe < CL_SIZE) {
        memcpy(dest, src, left_fringe);
        dest = (void *)((char *)dest + left_fringe);
        src = (void *)((char *)src + left_fringe);
    }
    while(len > 0) {
        // Calculate remaining size in page for src and dest
        uint64_t src_off = PAGE_SIZE - ((uint64_t)src & (PAGE_SIZE - 1));
        uint64_t dest_off = PAGE_SIZE - ((uint64_t)dest & (PAGE_SIZE - 1));
        // Pick minimum size left as elide_size
        uint64_t elide_size = cust_min(cust_min(src_off, dest_off), len);
        m5_memcpy_elide(dest, src, elide_size);
        dest = (void *)((char *)dest + elide_size);
        src = (void *)((char *)src + elide_size);
        len -= elide_size;
    }
}

void memcpy_elide_clwb(void* dest, void* src, uint64_t len)
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
        memcpy(dest, src, left_fringe);
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
          memcpy(dest, (void*)temp_src, elide_size);
        }
        else {
          // Make elide size a multiple of 64
          elide_size &= (~63);
          m5_memcpy_elide(dest, (void*)temp_src, elide_size);
        }
        dest = (void *)((char *)dest + elide_size);
        temp_src = (temp_src + elide_size);
        len -= elide_size;
    }
    _mm_mfence();
}

void memcpy_elide_free(void* dest, uint64_t len)
{
    /*void *temp_dest = (void*)((uint64_t)dest & ~((uint64_t)63));
    uint64_t pages = (len >> PAGE_BITS) + (len & ((1 << PAGE_BITS) - 1) ? 1 : 0);
    for(uint64_t page = 0; page < pages; ++page) {
        m5_memcpy_elide_free((void*)((uint64_t)temp_dest + (page << PAGE_BITS)), PAGE_SIZE);
    }
    _mm_mfence();*/
    m5_memcpy_elide_free(dest, 1);
    _mm_mfence();
}
" > test_headers.h

echo "
#include \"test_headers.h\"
int main(int argc, char *argv[])
{
    size_t size = SIZE;
    uint64_t *test1 = (uint64_t*)aligned_alloc(PAGE_SIZE, size + 16);
    uint64_t *test2 = (uint64_t*)aligned_alloc(PAGE_SIZE, size);
    test1 = (uint64_t*)((uint64_t)test1 + 16);
    init_op(test2, test1, size);
    printf(\"%p\n\", test1);
    printf(\"%p\n\", test2);
    TEST_OP(memcpy_elide_clwb, test2, test1, size, 0);
    TEST_OP(memcpy_elide_clwb, test2, test1, size, ACCESSES / 8);
    TEST_OP(memcpy_elide_clwb, test2, test1, size, ACCESSES / 4);
    TEST_OP(memcpy_elide_clwb, test2, test1, size, ACCESSES / 2);
    TEST_OP(memcpy_elide_clwb, test2, test1, size, ACCESSES);
    TEST_OP(memcpy_elide_clwb, test2, test1, size, (2 * ACCESSES));
    return 0;
}
" > test_clwb.cpp;

echo "
#include \"test_headers.h\"
int main(int argc, char *argv[])
{
    size_t size = SIZE;
    uint64_t *test1 = (uint64_t*)aligned_alloc(PAGE_SIZE, size + 16);
    uint64_t *test2 = (uint64_t*)aligned_alloc(PAGE_SIZE, size);
    test1 = (uint64_t*)((uint64_t)test1 + 16);
    init_op(test2, test1, size);
    printf(\"%p\n\", test1);
    printf(\"%p\n\", test2);
    TEST_OP(memcpy, test2, test1, size, 0);
    TEST_OP(memcpy, test2, test1, size, ACCESSES / 8);
    TEST_OP(memcpy, test2, test1, size, ACCESSES / 4);
    TEST_OP(memcpy, test2, test1, size, ACCESSES / 2);
    TEST_OP(memcpy, test2, test1, size, ACCESSES);
    TEST_OP(memcpy, test2, test1, size, (2 * ACCESSES));
    return 0;
}
" > test_memcpy.cpp;
tests="test_clwb test_memcpy"
for i in $tests; do
    g++ $i.cpp -o $i -lrt -g -march=native -I../include ../util/m5/build/x86/out/libm5.a
done

ZIO=/home/akkamath/zIO
ZIO_BIN=${ZIO}/copy_interpose.so

pushd ${ZIO};
make
ls
popd;

echo "Done compilation"
m5 exit
for i in $tests; do
    ./$i
done
LD_PRELOAD=${ZIO_BIN} ./test_memcpy
m5 exit
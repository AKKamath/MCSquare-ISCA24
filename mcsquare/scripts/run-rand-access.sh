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
#define CL_BITS 6
#define ACCESSES (SIZE / sizeof(uint64_t))

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

#define TEST_OP(OPERATION) \
    reset_op(test2, test1, size); \
    _mm_mfence();  \
    OPERATION; \
    _mm_mfence(); \
    printf(\"Dest: %lu Src: %lu\n\", *test2, *test1); \
    _mm_mfence(); \
    m5_reset_stats(0, 0); \
    random_test(test2, test1, size); \
    m5_dump_stats(0, 0); \
    memcpy_elide_free(test2, size);


void reset_op(uint64_t* dest, uint64_t* src, uint64_t size) {
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

void random_test(uint64_t* dest, uint64_t* src, uint64_t size) {
    //auto start = TIME_NOW;
    uint64_t index = 0;
    for(uint64_t i = 0; i < ACCESSES; i++) {
        index = dest[index];
    }
    //auto stop = TIME_NOW;
    printf(\"Verify: %lu\n\", index);
}

void memcpy_elide_pgflush(void* dest, void* src, uint64_t len)
{
    void *temp_dest = (void*)((uint64_t)dest & ~((uint64_t)63));
    void *temp_src = (void*)((uint64_t)src & ~((uint64_t)63));
    uint64_t pages = (len >> PAGE_BITS) + (len & ((1 << PAGE_BITS) - 1) ? 1 : 0);
    for (uint64_t page = 0; page < pages; ++page) {
        _mm_clwb( (void*)((uint64_t)temp_src + (page << PAGE_BITS)) );
        _mm_mfence();
        m5_memcpy_elide((void*)((uint64_t)temp_dest + (page << PAGE_BITS)),
            (void*)((uint64_t)temp_src + (page << PAGE_BITS)), PAGE_SIZE);
    }
}

void memcpy_elide_clflush(void* dest, void* src, uint64_t len)
{
    void *temp_dest = (void*)((uint64_t)dest & ~((uint64_t)63));
    void *temp_src = (void*)((uint64_t)src & ~((uint64_t)63));
    uint64_t pages = (len >> PAGE_BITS) + (len & ((1 << PAGE_BITS) - 1) ? 1 : 0);
    for(uint64_t page = 0; page < pages; ++page) {
        for (uint64_t i = 0; i < 64; ++i) {
            uint64_t offset = (i << CL_BITS) + (page << PAGE_BITS);
            _mm_clwb( (void*)((uint64_t)temp_src + offset) );
        }
        _mm_mfence();
        m5_memcpy_elide((void*)((uint64_t)temp_dest + (page << PAGE_BITS)), 
            (void*)((uint64_t)temp_src + (page << PAGE_BITS)), PAGE_SIZE);
    }
}

void memcpy_elide_clflush_src(void* dest, void* src, uint64_t len)
{
    void *temp_dest = (void*)((uint64_t)dest & ~((uint64_t)63));
    void *temp_src = (void*)((uint64_t)src & ~((uint64_t)63));
    uint64_t pages = (len >> PAGE_BITS) + (len & ((1 << PAGE_BITS) - 1) ? 1 : 0);
    for(uint64_t page = 0; page < pages; ++page) {
        for (uint64_t i = 0; i < 64; ++i) {
            uint64_t offset = (i << CL_BITS) + (page << PAGE_BITS);
            _mm_clwb( (void*)((uint64_t)temp_src + offset) );
        }
        _mm_mfence();
        m5_memcpy_elide((void*)((uint64_t)temp_dest + (page << PAGE_BITS)), 
            (void*)((uint64_t)temp_src + (page << PAGE_BITS)), PAGE_SIZE);
    }
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
    uint64_t *test1 = (uint64_t*)mmap(NULL, size, PROT_READ | PROT_WRITE, 
        MAP_POPULATE | MAP_ANONYMOUS | MAP_SHARED, -1, 0);
    uint64_t *test2 = (uint64_t*)mmap(NULL, size, PROT_READ | PROT_WRITE, 
        MAP_POPULATE | MAP_ANONYMOUS | MAP_SHARED, -1, 0);
    printf(\"%p\n\", test1);
    printf(\"%p\n\", test2);
    TEST_OP(memcpy_elide_pgflush(test2, test1, size));
    return 0;
}
" > test_pgflush.cpp;
echo "
#include \"test_headers.h\"
int main(int argc, char *argv[])
{
    size_t size = SIZE;
    uint64_t *test1 = (uint64_t*)mmap(NULL, size, PROT_READ | PROT_WRITE, 
        MAP_POPULATE | MAP_ANONYMOUS | MAP_SHARED, -1, 0);
    uint64_t *test2 = (uint64_t*)mmap(NULL, size, PROT_READ | PROT_WRITE, 
        MAP_POPULATE | MAP_ANONYMOUS | MAP_SHARED, -1, 0);
    printf(\"%p\n\", test1);
    printf(\"%p\n\", test2);
    TEST_OP(memcpy_elide_clflush(test2, test1, size));
    return 0;
}
" > test_clflush.cpp;
echo "
#include \"test_headers.h\"
int main(int argc, char *argv[])
{
    size_t size = SIZE;
    uint64_t *test1 = (uint64_t*)mmap(NULL, size, PROT_READ | PROT_WRITE, 
        MAP_POPULATE | MAP_ANONYMOUS | MAP_SHARED, -1, 0);
    uint64_t *test2 = (uint64_t*)mmap(NULL, size, PROT_READ | PROT_WRITE, 
        MAP_POPULATE | MAP_ANONYMOUS | MAP_SHARED, -1, 0);
    printf(\"%p\n\", test1);
    printf(\"%p\n\", test2);
    TEST_OP(memcpy_elide_clflush_src(test2, test1, size));
    return 0;
}
" > test_clflush_src.cpp;
echo "
#include \"test_headers.h\"
int main(int argc, char *argv[])
{
    size_t size = SIZE;
    uint64_t *test1 = (uint64_t*)aligned_alloc(PAGE_SIZE, size);
    uint64_t *test2 = (uint64_t*)aligned_alloc(PAGE_SIZE, size);
    printf(\"%p\n\", test1);
    printf(\"%p\n\", test2);
    TEST_OP(memcpy(test2, test1, size));
    return 0;
}
" > test_memcpy.cpp;
tests="test_clflush test_clflush_src test_memcpy"
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
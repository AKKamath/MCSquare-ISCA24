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
#define PAGE_SIZE 4096
#define CL_SIZE 64
#define SIZE (PAGE_SIZE)
#define PAGE_BITS 12
#define CL_BITS 6
#define ACCESSES (32)

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

#define TEST_OP(OPERATION, DEST, SRC, SIZE_VAL) \
    reset_op(DEST, SRC, SIZE_VAL); \
    _mm_mfence();  \
    OPERATION(DEST, SRC, SIZE_VAL); \
    _mm_mfence(); \
    m5_reset_stats(0, 0); \
    random_test(DEST, SRC, SIZE_VAL); \
    m5_dump_stats(0, 0); \
    memcpy_elide_free(DEST, SIZE_VAL);


void reset_op(uint64_t* dest, uint64_t* src, uint64_t size) {
    // Set initial values
    for(int i = 0; i < size / sizeof(uint64_t); i++) {
        src[i] = i;
        dest[i] = 0;
    }
}

void random_test(uint64_t* dest, uint64_t* src, uint64_t size) {
    uint64_t index = 0;
    for(uint64_t i = 0; i < ACCESSES; i++) {
        printf(\"%lu \", dest[i]);
    }
    printf(\"\n\");
}

void memcpy_elide_pgwb(void* dest, void* src, uint64_t len)
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

void memcpy_elide_clwb(void* dest, void* src, uint64_t len)
{
    uint64_t temp_src = (void*)((uint64_t)src & ~((uint64_t)63));
    while(temp_src < (uint64_t)src + len) {
        _mm_clwb( (void*)temp_src );
        temp_src += CL_SIZE;
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
    printf(\"Pgflush\t\");
    size_t size = SIZE;
    uint64_t *src = (uint64_t*)mmap(NULL, size, PROT_READ | PROT_WRITE, 
        MAP_POPULATE | MAP_ANONYMOUS | MAP_SHARED, -1, 0);
    uint64_t *dest = (uint64_t*)mmap(NULL, size, PROT_READ | PROT_WRITE, 
        MAP_POPULATE | MAP_ANONYMOUS | MAP_SHARED, -1, 0);
    printf(\"Dest: %p\n\", dest);
    printf(\"Src: %p\n\", src);
    TEST_OP(memcpy_elide_pgflush, dest, src, sizeof(uint64_t) * ACCESSES);

    src = (uint64_t*)((char*)src + 16);
    printf(\"Dest: %p\n\", dest);
    printf(\"Src: %p\n\", src);
    TEST_OP(memcpy_elide_pgflush, dest, src, sizeof(uint64_t) * ACCESSES);
    return 0;
}
" > test_pgflush.cpp;
echo "
#include \"test_headers.h\"
int main(int argc, char *argv[])
{
    printf(\"Srcflush\n\");
    size_t size = SIZE;
    uint64_t *src = (uint64_t*)mmap(NULL, size, PROT_READ | PROT_WRITE, 
        MAP_POPULATE | MAP_ANONYMOUS | MAP_SHARED, -1, 0);
    uint64_t *dest = (uint64_t*)mmap(NULL, size, PROT_READ | PROT_WRITE, 
        MAP_POPULATE | MAP_ANONYMOUS | MAP_SHARED, -1, 0);
    printf(\"Dest: %p\n\", dest);
    printf(\"Src: %p\n\", src);
    TEST_OP(memcpy_elide_clflush_src, dest, src, sizeof(uint64_t) * ACCESSES);

    src = (uint64_t*)((char*)src + 16);
    printf(\"Dest: %p\n\", dest);
    printf(\"Src: %p\n\", src);
    TEST_OP(memcpy_elide_clflush_src, dest, src, sizeof(uint64_t) * ACCESSES);
    return 0;
}
" > test_clflush_src.cpp;
echo "
#include \"test_headers.h\"
int main(int argc, char *argv[])
{
    printf(\"Memcpy\n\");
    size_t size = SIZE;
    uint64_t *src = (uint64_t*)mmap(NULL, size, PROT_READ | PROT_WRITE, 
        MAP_POPULATE | MAP_ANONYMOUS | MAP_SHARED, -1, 0);
    uint64_t *dest = (uint64_t*)mmap(NULL, size, PROT_READ | PROT_WRITE, 
        MAP_POPULATE | MAP_ANONYMOUS | MAP_SHARED, -1, 0);
    printf(\"Dest: %p\n\", dest);
    printf(\"Src: %p\n\", src);
    TEST_OP(memcpy, dest, src, sizeof(uint64_t) * ACCESSES);

    src = (uint64_t*)((char*)src + 16);
    printf(\"Dest: %p\n\", dest);
    printf(\"Src: %p\n\", src);
    TEST_OP(memcpy, dest, src, sizeof(uint64_t) * ACCESSES);
    return 0;
}
" > test_memcpy.cpp;
tests="test_clflush_src test_memcpy"
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
echo "zIO"
LD_PRELOAD=${ZIO_BIN} ./test_memcpy
m5 exit
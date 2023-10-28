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
#include <sys/syscall.h>         /* Definition of SYS_* constants */
#include <sys/socket.h>
#include <string.h>
#define SIZE (1024*1024)
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

#define TEST_OP(OPERATION, dest, src, size, accesses) \
    reset_op(dest, src, size); \
    _mm_mfence(); \
    OPERATION(dest, src, size); \
    m5_reset_stats(0, 0); \
    sequential_test(dest, src, size, accesses); \
    _mm_mfence(); \
    m5_dump_stats(0, 0); \
    verify(dest, src, size, accesses); \
    memcpy_elide_free(dest, size);

void reset_op(uint64_t* dest, uint64_t* src, uint64_t size) {
    for(int i = 0; i < size / sizeof(uint64_t); i++) {
        src[i]  = i;
        dest[i] = 100;
    }
}

void sequential_test(uint64_t* dest, uint64_t* src, uint64_t size, uint64_t accesses) {
    for(int i = 0; i < accesses; i++) {
        src[i] = 200 + i;
    }

    for(int i = 0; i < accesses; i += CL_SIZE / sizeof(uint64_t)) {
        _mm_clflushopt(&src[i]);
    }
    _mm_mfence();
}

void verify(uint64_t* dest, uint64_t* src, uint64_t size, uint64_t accesses) {
    bool correct = true;
    for(int i = 0; i < accesses; i += CL_SIZE / sizeof(uint64_t)) {
        if(dest[i] != i) {
            correct = false;
            printf(\"Dest: Found %lu, expected %d\n\", dest[i], i);
        }
    }
    for(int i = 0; i < accesses; i += CL_SIZE / sizeof(uint64_t)) {
        if(src[i] != 200 + i) {
            correct = false;
            printf(\"Src: Found %lu, expected %d\n\", src[i], i);
        }
    }
    printf(\"Correct? %d\n\", correct);
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
    printf(\"%p\n\", test1);
    printf(\"%p\n\", test2);
    TEST_OP(memcpy_elide_clwb, test2, test1, size, ACCESSES / 8);
    TEST_OP(memcpy_elide_clwb, test2, test1, size, ACCESSES / 4);
    TEST_OP(memcpy_elide_clwb, test2, test1, size, ACCESSES / 2);
    TEST_OP(memcpy_elide_clwb, test2, test1, size, ACCESSES);
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
    printf(\"%p\n\", test1);
    printf(\"%p\n\", test2);
    TEST_OP(memcpy, test2, test1, size, ACCESSES / 8);
    TEST_OP(memcpy, test2, test1, size, ACCESSES / 4);
    TEST_OP(memcpy, test2, test1, size, ACCESSES / 2);
    TEST_OP(memcpy, test2, test1, size, ACCESSES);
    return 0;
}
" > test_memcpy.cpp;

tests="test_clwb"
for i in $tests; do
    g++ $i.cpp -o $i -lrt -g -march=native -I../include ../util/m5/build/x86/out/libm5.a
done

echo "Done compilation"
m5 exit

for i in $tests; do
    ./$i
done
m5 exit
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

#define TEST_OP(OPERATION, dest, src, size, accesses) \
    reset_op(dest, src, size); \
    _mm_mfence();  \
    OPERATION(dest, src, size); \
    _mm_mfence(); \
    printf(\"Dest: %lu Src: %lu\n\", *dest, *src); \
    _mm_mfence(); \
    m5_reset_stats(0, 0); \
    sequential_test(dest, src, size, accesses); \
    m5_dump_stats(0, 0); \
    memcpy_elide_free(dest, size);


void reset_op(uint64_t* dest, uint64_t* src, uint64_t size) {
    for(int i = 0; i < size / sizeof(uint64_t); i += PAGE_SIZE / sizeof(uint64_t)) {
        src[i]  = 500;
        dest[i] = 100;
    }
}

void sequential_test(uint64_t* dest, uint64_t* src, uint64_t size, uint64_t accesses) {
    //auto start = TIME_NOW;
    uint64_t verify = 0;
    for(int i = 0; i < accesses; i++) {
        verify += dest[i];
    }
    //auto stop = TIME_NOW;
    printf(\"Verify: %llu\n\", verify);
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
    size_t size = SIZE;
    uint64_t *test1 = (uint64_t*)mmap(NULL, size, PROT_READ | PROT_WRITE, 
        MAP_POPULATE | MAP_ANONYMOUS | MAP_SHARED, -1, 0);
    uint64_t *test2 = (uint64_t*)mmap(NULL, size, PROT_READ | PROT_WRITE, 
        MAP_POPULATE | MAP_ANONYMOUS | MAP_SHARED, -1, 0);
    printf(\"%p\n\", test1);
    printf(\"%p\n\", test2);
    TEST_OP(memcpy_elide_pgflush, test2, test1, size, ACCESSES / 10);
    TEST_OP(memcpy_elide_pgflush, test2, test1, size, ACCESSES / 5);
    TEST_OP(memcpy_elide_pgflush, test2, test1, size, ACCESSES / 2);
    TEST_OP(memcpy_elide_pgflush, test2, test1, size, ACCESSES);
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
    TEST_OP(memcpy_elide_clwb, test2, test1, size, ACCESSES / 10);
    TEST_OP(memcpy_elide_clwb, test2, test1, size, ACCESSES / 5);
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
    uint64_t *test1 = (uint64_t*)aligned_alloc(PAGE_SIZE, size);
    uint64_t *test2 = (uint64_t*)aligned_alloc(PAGE_SIZE, size);
    printf(\"%p\n\", test1);
    printf(\"%p\n\", test2);
    TEST_OP(memcpy, test2, test1, size, ACCESSES / 10);
    TEST_OP(memcpy, test2, test1, size, ACCESSES / 5);
    TEST_OP(memcpy, test2, test1, size, ACCESSES / 2);
    TEST_OP(memcpy, test2, test1, size, ACCESSES);
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
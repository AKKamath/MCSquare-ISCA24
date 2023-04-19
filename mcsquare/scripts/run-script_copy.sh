cd /home/akkamath/gem5-zIO/util/m5
scons build/x86/out
cd /home/akkamath/gem5-zIO/zio_stuff
ls
echo "
#include <gem5/m5ops.h>
#include <stdio.h>
#include <stdlib.h>
#include <x86intrin.h>
#include <string.h>
#define SIZE (1024 * 1024)
#define PAGE_SIZE 4096
#define PAGE_BITS 12
#define CL_BITS 6

#define TEST_OP(OPERATION) \
    _mm_mfence();  \
    m5_reset_stats(0, 0); \
    OPERATION; \
    _mm_mfence(); \
    m5_dump_stats(0, 0);


__inline__ void memcpy_elide_pgflush(void* dest, void* src, uint64_t len)
{
    void *temp_dest = (void*)((uint64_t)dest & ~((uint64_t)63));
    void *temp_src = (void*)((uint64_t)src & ~((uint64_t)63));
    uint64_t pages = (len >> PAGE_BITS) + (len & ((1 << PAGE_BITS) - 1) ? 1 : 0);
    for (uint64_t page = 0; page < pages; ++page) {
        _mm_clflushopt( (void*)((uint64_t)temp_dest + (page << PAGE_BITS)) );
        _mm_clflushopt( (void*)((uint64_t)temp_src + (page << PAGE_BITS)) );
        _mm_mfence();
        m5_memcpy_elide((void*)((uint64_t)temp_dest + (page << PAGE_BITS)),
            (void*)((uint64_t)temp_src + (page << PAGE_BITS)), PAGE_SIZE);
    }
}

__inline__ void memcpy_elide_clflush(void* dest, void* src, uint64_t len)
{
    void *temp_dest = (void*)((uint64_t)dest & ~((uint64_t)63));
    void *temp_src = (void*)((uint64_t)src & ~((uint64_t)63));
    uint64_t pages = (len >> PAGE_BITS) + (len & ((1 << PAGE_BITS) - 1) ? 1 : 0);
    for(uint64_t page = 0; page < pages; ++page) {
        for (uint64_t i = 0; i < 64; ++i) {
            uint64_t offset = (i << CL_BITS) + (page << PAGE_BITS);
            _mm_clflushopt( (void*)((uint64_t)temp_dest + offset) );
            _mm_clflushopt( (void*)((uint64_t)temp_src + offset) );
        }
        _mm_mfence();
        m5_memcpy_elide((void*)((uint64_t)temp_dest + (page << PAGE_BITS)), 
            (void*)((uint64_t)temp_src + (page << PAGE_BITS)), PAGE_SIZE);
    }
}

__inline__ void memcpy_elide_clflush_src(void* dest, void* src, uint64_t len)
{
    void *temp_dest = (void*)((uint64_t)dest & ~((uint64_t)63));
    void *temp_src = (void*)((uint64_t)src & ~((uint64_t)63));
    uint64_t pages = (len >> PAGE_BITS) + (len & ((1 << PAGE_BITS) - 1) ? 1 : 0);
    for(uint64_t page = 0; page < pages; ++page) {

        _mm_clflushopt( (void*)((uint64_t)temp_dest + (page << PAGE_BITS)) );
        for (uint64_t i = 0; i < 64; ++i) {
            uint64_t offset = (i << CL_BITS) + (page << PAGE_BITS);
            _mm_clflushopt( (void*)((uint64_t)temp_src + offset) );
        }
        _mm_mfence();
        m5_memcpy_elide((void*)((uint64_t)temp_dest + (page << PAGE_BITS)), 
            (void*)((uint64_t)temp_src + (page << PAGE_BITS)), PAGE_SIZE);
    }
}

int main(int argc, char *argv[])
{
    size_t size = SIZE;
    int *test1 = (int*)aligned_alloc(PAGE_SIZE, size);
    int *test2 = (int*)aligned_alloc(PAGE_SIZE, size);
    printf(\"%p\n\", test1);
    printf(\"%p\n\", test2);

    for(int i = 0; i < size / sizeof(int); i += PAGE_SIZE / sizeof(int)) {
        test2[i] = 500;
        test1[i] = 100;
    }
    printf(\"%d %d\n\", *test2, *test1);

    TEST_OP(memcpy(test2, test1, size))
    TEST_OP(memcpy_elide_pgflush(test2, test1, size));
    TEST_OP(memcpy_elide_clflush(test2, test1, size));
    TEST_OP(memcpy_elide_clflush_src(test2, test1, size));
    
    TEST_OP(memcpy(test2, test1, size / 4));
    TEST_OP(memcpy_elide_pgflush(test2, test1, size / 4));
    TEST_OP(memcpy_elide_clflush(test2, test1, size / 4));
    TEST_OP(memcpy_elide_clflush_src(test2, test1, size / 4));
    
    TEST_OP(memcpy(test2, test1, 4096));
    TEST_OP(memcpy_elide_pgflush(test2, test1, 4096));
    TEST_OP(memcpy_elide_clflush(test2, test1, 4096));
    TEST_OP(memcpy_elide_clflush_src(test2, test1, 4096));

    return 0;
}
" > test_file.cpp;
#echo "run; bt; quit;" > run.sh
g++ test_file.cpp -o test_file -lrt -g -march=native -I../include ../util/m5/build/x86/out/libm5.a
sleep 5
echo "HERE"
#objdump -D test_file
m5 exit
numactl -C 0 ./test_file
m5 exit
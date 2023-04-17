cd /home/akkamath/gem5-zIO/util/m5
scons build/x86/out
cd /home/akkamath/gem5-zIO/zio_stuff
ls
#make copy_sweep
#./copy_sweep 4 4096 4
#g++ copy_sweep2.cpp -o copy_sweep2 -lrt -g  -I../include ../util/m5/build/x86/out/libm5.a -lpthread -pthread
#./copy_sweep2

echo "
#include <gem5/m5ops.h>
#include <stdio.h>
#include <stdlib.h>
#include <x86intrin.h>
#include <string.h>
#define PAGE_SIZE 2048
#define TEST_OP(OPERATION) \
    _mm_mfence();  \
    m5_reset_stats(0, 0); \
    OPERATION; \
    _mm_mfence(); \
    m5_dump_stats(0, 0);


void memcpy_elide2(void* dest, void* src, uint64_t len)
{
    void *temp_dest = (void*)((uint64_t)dest & ~((uint64_t)63));
    void *temp_src = (void*)((uint64_t)src & ~((uint64_t)63));
    uint64_t temp_len = (len >> 6) + 1;
    for (int i = 0; i < 1; ++i)
    {
        _mm_clflushopt( (void*)((uint64_t)temp_dest + (i << 6)) );
        _mm_clflushopt( (void*)((uint64_t)temp_src + (i << 6)) );
    }
    _mm_mfence();
    //m5_memcpy_elide(dest, src, len);
}

void memcpy_elide3(void* dest, void* src, uint64_t len)
{
    void *temp_dest = (void*)((uint64_t)dest & ~((uint64_t)63));
    void *temp_src = (void*)((uint64_t)src & ~((uint64_t)63));
    uint64_t temp_len = (len >> 6) + 1;
    for (int i = 0; i < temp_len; ++i)
    {
        _mm_clflushopt( (void*)((uint64_t)temp_dest + (i << 6)) );
        _mm_clflushopt( (void*)((uint64_t)temp_src + (i << 6)) );
    }
    _mm_mfence();
    //m5_memcpy_elide(dest, src, len);
}

int main(int argc, char *argv[])
{
    int *test1 = 0, *test2 = 0;
    size_t size = PAGE_SIZE;
    test1 = (int*)aligned_alloc(PAGE_SIZE, size);
    test2 = (int*)aligned_alloc(PAGE_SIZE, size);
    printf(\"%p\n\", test1);
    printf(\"%p\n\", test2);
    *(int*)test1 = 500;
    *(int*)test2 = 100;
    printf(\"%d %d\n\", *test2, *test1);

    TEST_OP(memcpy(test2, test1, size))
    printf(\"%d %d\n\", *test2, *test1);

    TEST_OP(memcpy_elide2(test2, test1, size));
    printf(\"%d %d\n\", *test2, *test1);

    TEST_OP(memcpy_elide3(test2, test1, size));
    printf(\"%d %d\n\", *test2, *test1);
    
    TEST_OP(memcpy(test2, test1, size / 2));
    printf(\"%d %d\n\", *test2, *test1);

    TEST_OP(memcpy_elide2(test2, test1, size / 2));
    printf(\"%d %d\n\", *test2, *test1);

    TEST_OP(memcpy_elide3(test2, test1, size / 2));
    printf(\"%d %d\n\", *test2, *test1);
    
    TEST_OP(memcpy(test2, test1, 64));
    printf(\"%d %d\n\", *test2, *test1);

    TEST_OP(memcpy_elide2(test2, test1, 64));
    printf(\"%d %d\n\", *test2, *test1);

    TEST_OP(memcpy_elide3(test2, test1, 64));
    printf(\"%d %d\n\", *test2, *test1);

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
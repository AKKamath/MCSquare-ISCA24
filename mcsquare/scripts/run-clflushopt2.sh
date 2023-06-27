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
#define PAGE_SIZE 4096
#define SIZE (4096 * 4096)
#define PAGE_BITS 12
#define CL_BITS 6

#define TEST_OP(OPERATION) \
    _mm_mfence(); \
    m5_reset_stats(0, 0); \
    OPERATION; \
    _mm_mfence(); \
    m5_dump_stats(0, 0); \
    offset += (size << 6); \
    test1 = (int*)((uint64_t)orig + (offset % SIZE));

void clflushopt(void* dest, uint64_t len)
{
    for(uint64_t i = 0; i < len; ++i) {
        _mm_clflushopt( (void*)((uint64_t)dest + (i << 6)) );
        _mm_mfence();
    }
}

void clflushopt4(void* dest, uint64_t len)
{
    for(uint64_t i = 0; i < len / 4; ++i) {
        _mm_clflushopt( (void*)((uint64_t)dest + ((i * 4) << 6)) );
        _mm_clflushopt( (void*)((uint64_t)dest + ((i * 4 + 1) << 6)) );
        _mm_clflushopt( (void*)((uint64_t)dest + ((i * 4 + 2) << 6)) );
        _mm_clflushopt( (void*)((uint64_t)dest + ((i * 4 + 3) << 6)) );
        _mm_mfence();
    }
}

void clflushopt16(void* dest, uint64_t len)
{
    for(uint64_t i = 0; i < len / 16; ++i) {
        _mm_clflushopt( (void*)((uint64_t)dest + ((i * 16) << 6)) );
        _mm_clflushopt( (void*)((uint64_t)dest + ((i * 16 + 1) << 6)) );
        _mm_clflushopt( (void*)((uint64_t)dest + ((i * 16 + 2) << 6)) );
        _mm_clflushopt( (void*)((uint64_t)dest + ((i * 16 + 3) << 6)) );
        _mm_clflushopt( (void*)((uint64_t)dest + ((i * 16 + 4) << 6)) );
        _mm_clflushopt( (void*)((uint64_t)dest + ((i * 16 + 5) << 6)) );
        _mm_clflushopt( (void*)((uint64_t)dest + ((i * 16 + 6) << 6)) );
        _mm_clflushopt( (void*)((uint64_t)dest + ((i * 16 + 7) << 6)) );
        _mm_clflushopt( (void*)((uint64_t)dest + ((i * 16 + 8) << 6)) );
        _mm_clflushopt( (void*)((uint64_t)dest + ((i * 16 + 9) << 6)) );
        _mm_clflushopt( (void*)((uint64_t)dest + ((i * 16 + 10) << 6)) );
        _mm_clflushopt( (void*)((uint64_t)dest + ((i * 16 + 11) << 6)) );
        _mm_clflushopt( (void*)((uint64_t)dest + ((i * 16 + 12) << 6)) );
        _mm_clflushopt( (void*)((uint64_t)dest + ((i * 16 + 13) << 6)) );
        _mm_clflushopt( (void*)((uint64_t)dest + ((i * 16 + 14) << 6)) );
        _mm_clflushopt( (void*)((uint64_t)dest + ((i * 16 + 15) << 6)) );
        _mm_mfence();
    }
}

void clflushopt32(void* dest, uint64_t len)
{
    for(uint64_t i = 0; i < len / 32; ++i) {
        _mm_clflushopt( (void*)((uint64_t)dest + ((i * 32) << 6)) );
        _mm_clflushopt( (void*)((uint64_t)dest + ((i * 32 + 1) << 6)) );
        _mm_clflushopt( (void*)((uint64_t)dest + ((i * 32 + 2) << 6)) );
        _mm_clflushopt( (void*)((uint64_t)dest + ((i * 32 + 3) << 6)) );
        _mm_clflushopt( (void*)((uint64_t)dest + ((i * 32 + 4) << 6)) );
        _mm_clflushopt( (void*)((uint64_t)dest + ((i * 32 + 5) << 6)) );
        _mm_clflushopt( (void*)((uint64_t)dest + ((i * 32 + 6) << 6)) );
        _mm_clflushopt( (void*)((uint64_t)dest + ((i * 32 + 7) << 6)) );
        _mm_clflushopt( (void*)((uint64_t)dest + ((i * 32 + 8) << 6)) );
        _mm_clflushopt( (void*)((uint64_t)dest + ((i * 32 + 9) << 6)) );
        _mm_clflushopt( (void*)((uint64_t)dest + ((i * 32 + 10) << 6)) );
        _mm_clflushopt( (void*)((uint64_t)dest + ((i * 32 + 11) << 6)) );
        _mm_clflushopt( (void*)((uint64_t)dest + ((i * 32 + 12) << 6)) );
        _mm_clflushopt( (void*)((uint64_t)dest + ((i * 32 + 13) << 6)) );
        _mm_clflushopt( (void*)((uint64_t)dest + ((i * 32 + 14) << 6)) );
        _mm_clflushopt( (void*)((uint64_t)dest + ((i * 32 + 15) << 6)) );
        _mm_clflushopt( (void*)((uint64_t)dest + ((i * 32 + 16) << 6)) );
        _mm_clflushopt( (void*)((uint64_t)dest + ((i * 32 + 17) << 6)) );
        _mm_clflushopt( (void*)((uint64_t)dest + ((i * 32 + 18) << 6)) );
        _mm_clflushopt( (void*)((uint64_t)dest + ((i * 32 + 19) << 6)) );
        _mm_clflushopt( (void*)((uint64_t)dest + ((i * 32 + 20) << 6)) );
        _mm_clflushopt( (void*)((uint64_t)dest + ((i * 32 + 21) << 6)) );
        _mm_clflushopt( (void*)((uint64_t)dest + ((i * 32 + 22) << 6)) );
        _mm_clflushopt( (void*)((uint64_t)dest + ((i * 32 + 23) << 6)) );
        _mm_clflushopt( (void*)((uint64_t)dest + ((i * 32 + 24) << 6)) );
        _mm_clflushopt( (void*)((uint64_t)dest + ((i * 32 + 25) << 6)) );
        _mm_clflushopt( (void*)((uint64_t)dest + ((i * 32 + 26) << 6)) );
        _mm_clflushopt( (void*)((uint64_t)dest + ((i * 32 + 27) << 6)) );
        _mm_clflushopt( (void*)((uint64_t)dest + ((i * 32 + 28) << 6)) );
        _mm_clflushopt( (void*)((uint64_t)dest + ((i * 32 + 29) << 6)) );
        _mm_clflushopt( (void*)((uint64_t)dest + ((i * 32 + 30) << 6)) );
        _mm_clflushopt( (void*)((uint64_t)dest + ((i * 32 + 31) << 6)) );
        _mm_mfence();
    }
}

void page_fault_all(int *test, uint64_t size) {
    for(int i = 0; i < size / sizeof(int); i += PAGE_SIZE / sizeof(int)) {
        test[i] = 0;
    }
}
" > test_headers.h

sizes=(64 256) # 16 64 256 1024
for i in ${sizes[@]}; do
    echo ${i};
    echo "
    #include \"test_headers.h\"
    int main(int argc, char *argv[])
    {
        size_t size = ${i};
        int *test1, *orig;
        uint64_t offset = 0;
        orig = test1 = (int*)mmap(NULL, SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE, -1, 0);
        printf(\"%p\n\", test1);
        page_fault_all(test1, SIZE);
        
        TEST_OP(clflushopt(test1, size));
        TEST_OP(clflushopt4(test1, size));
        TEST_OP(clflushopt16(test1, size));
        TEST_OP(clflushopt32(test1, size));
        return 0;
    }
    " > test_all_${i}.cpp;
    g++ test_all_$i.cpp -o test_all_$i -lrt -g -march=native -I../include ../util/m5/build/x86/out/libm5.a
done

#objdump -d test_all_4

echo "Done compilation"
m5 exit

# Begin tests
for i in ${sizes[@]}; do
    echo "Test: size $i"
    ./test_all_$i
done

# All done!
m5 exit
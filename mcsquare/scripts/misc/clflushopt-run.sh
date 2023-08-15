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
    offset += (size); \
    test1 = (int*)((uint64_t)orig + (offset % SIZE));

void clflushopt(void* dest, uint64_t len)
{
    for(uint64_t i = 0; i < len; ++i) {
        _mm_clflushopt( (void*)((uint64_t)dest + (i << 6)) );
    }
}

void clflush(void* dest, uint64_t len)
{
    for(uint64_t i = 0; i < len; ++i) {
        _mm_clflush( (void*)((uint64_t)dest + (i << 6)) );
    }
}

void store(void* dest, uint64_t len)
{
    for(uint64_t i = 0; i < len; ++i) {
        *(int*)((uint64_t)dest + (i << 6)) = 0;
    }
}

void clflushopt4(void* dest, uint64_t len)
{
    for(uint64_t i = 0; i < len / 4; ++i) {
        _mm_clflushopt( (void*)((uint64_t)dest + ((i * 4) << 6)) );
        _mm_clflushopt( (void*)((uint64_t)dest + ((i * 4 + 1) << 6)) );
        _mm_clflushopt( (void*)((uint64_t)dest + ((i * 4 + 2) << 6)) );
        _mm_clflushopt( (void*)((uint64_t)dest + ((i * 4 + 3) << 6)) );
    }
}

void clflush4(void* dest, uint64_t len)
{
    for(uint64_t i = 0; i < len / 4; ++i) {
        _mm_clflush( (void*)((uint64_t)dest + ((i * 4) << 6)) );
        _mm_clflush( (void*)((uint64_t)dest + ((i * 4 + 1) << 6)) );
        _mm_clflush( (void*)((uint64_t)dest + ((i * 4 + 2) << 6)) );
        _mm_clflush( (void*)((uint64_t)dest + ((i * 4 + 3) << 6)) );
    }
}

void store4(void* dest, uint64_t len)
{
    for(uint64_t i = 0; i < len / 4; ++i) {
        *(int*)((uint64_t)dest + ((i * 4) << 6)) = 0;
        *(int*)((uint64_t)dest + ((i * 4 + 1) << 6)) = 0;
        *(int*)((uint64_t)dest + ((i * 4 + 2) << 6)) = 0;
        *(int*)((uint64_t)dest + ((i * 4 + 3) << 6)) = 0;
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
    }
}

void clwb16(void* dest, uint64_t len)
{
    for(uint64_t i = 0; i < len / 16; ++i) {
        _mm_clwb( (void*)((uint64_t)dest + ((i * 16) << 6)) );
        _mm_clwb( (void*)((uint64_t)dest + ((i * 16 + 1) << 6)) );
        _mm_clwb( (void*)((uint64_t)dest + ((i * 16 + 2) << 6)) );
        _mm_clwb( (void*)((uint64_t)dest + ((i * 16 + 3) << 6)) );
        _mm_clwb( (void*)((uint64_t)dest + ((i * 16 + 4) << 6)) );
        _mm_clwb( (void*)((uint64_t)dest + ((i * 16 + 5) << 6)) );
        _mm_clwb( (void*)((uint64_t)dest + ((i * 16 + 6) << 6)) );
        _mm_clwb( (void*)((uint64_t)dest + ((i * 16 + 7) << 6)) );
        _mm_clwb( (void*)((uint64_t)dest + ((i * 16 + 8) << 6)) );
        _mm_clwb( (void*)((uint64_t)dest + ((i * 16 + 9) << 6)) );
        _mm_clwb( (void*)((uint64_t)dest + ((i * 16 + 10) << 6)) );
        _mm_clwb( (void*)((uint64_t)dest + ((i * 16 + 11) << 6)) );
        _mm_clwb( (void*)((uint64_t)dest + ((i * 16 + 12) << 6)) );
        _mm_clwb( (void*)((uint64_t)dest + ((i * 16 + 13) << 6)) );
        _mm_clwb( (void*)((uint64_t)dest + ((i * 16 + 14) << 6)) );
        _mm_clwb( (void*)((uint64_t)dest + ((i * 16 + 15) << 6)) );
    }
}

void clflush16(void* dest, uint64_t len)
{
    for(uint64_t i = 0; i < len / 16; ++i) {
        _mm_clflush( (void*)((uint64_t)dest + ((i * 16) << 6)) );
        _mm_clflush( (void*)((uint64_t)dest + ((i * 16 + 1) << 6)) );
        _mm_clflush( (void*)((uint64_t)dest + ((i * 16 + 2) << 6)) );
        _mm_clflush( (void*)((uint64_t)dest + ((i * 16 + 3) << 6)) );
        _mm_clflush( (void*)((uint64_t)dest + ((i * 16 + 4) << 6)) );
        _mm_clflush( (void*)((uint64_t)dest + ((i * 16 + 5) << 6)) );
        _mm_clflush( (void*)((uint64_t)dest + ((i * 16 + 6) << 6)) );
        _mm_clflush( (void*)((uint64_t)dest + ((i * 16 + 7) << 6)) );
        _mm_clflush( (void*)((uint64_t)dest + ((i * 16 + 8) << 6)) );
        _mm_clflush( (void*)((uint64_t)dest + ((i * 16 + 9) << 6)) );
        _mm_clflush( (void*)((uint64_t)dest + ((i * 16 + 10) << 6)) );
        _mm_clflush( (void*)((uint64_t)dest + ((i * 16 + 11) << 6)) );
        _mm_clflush( (void*)((uint64_t)dest + ((i * 16 + 12) << 6)) );
        _mm_clflush( (void*)((uint64_t)dest + ((i * 16 + 13) << 6)) );
        _mm_clflush( (void*)((uint64_t)dest + ((i * 16 + 14) << 6)) );
        _mm_clflush( (void*)((uint64_t)dest + ((i * 16 + 15) << 6)) );
    }
}

void store16(void* dest, uint64_t len)
{
    for(uint64_t i = 0; i < len / 16; ++i) {
        *(int*)((uint64_t)dest + ((i * 16) << 6)) = 0;
        *(int*)((uint64_t)dest + ((i * 16 + 1) << 6)) = 0;
        *(int*)((uint64_t)dest + ((i * 16 + 2) << 6)) = 0;
        *(int*)((uint64_t)dest + ((i * 16 + 3) << 6)) = 0;
        *(int*)((uint64_t)dest + ((i * 16 + 4) << 6)) = 0;
        *(int*)((uint64_t)dest + ((i * 16 + 5) << 6)) = 0;
        *(int*)((uint64_t)dest + ((i * 16 + 6) << 6)) = 0;
        *(int*)((uint64_t)dest + ((i * 16 + 7) << 6)) = 0;
        *(int*)((uint64_t)dest + ((i * 16 + 8) << 6)) = 0;
        *(int*)((uint64_t)dest + ((i * 16 + 9) << 6)) = 0;
        *(int*)((uint64_t)dest + ((i * 16 + 10) << 6)) = 0;
        *(int*)((uint64_t)dest + ((i * 16 + 11) << 6)) = 0;
        *(int*)((uint64_t)dest + ((i * 16 + 12) << 6)) = 0;
        *(int*)((uint64_t)dest + ((i * 16 + 13) << 6)) = 0;
        *(int*)((uint64_t)dest + ((i * 16 + 14) << 6)) = 0;
        *(int*)((uint64_t)dest + ((i * 16 + 15) << 6)) = 0;
    }
}

int load(void* dest, uint64_t len)
{
    int count = 0;
    for(uint64_t i = 0; i < len; ++i) {
        count += *(int*)((uint64_t)dest + (i << 6));
    }
    return count;
}

int load4(void* dest, uint64_t len)
{
    int count = 0;
    for(uint64_t i = 0; i < len / 4; ++i) {
        count += *(int*)((uint64_t)dest + ((i * 4) << 6));
        count += *(int*)((uint64_t)dest + ((i * 4 + 1) << 6));
        count += *(int*)((uint64_t)dest + ((i * 4 + 2) << 6));
        count += *(int*)((uint64_t)dest + ((i * 4 + 3) << 6));
    }
    return count;
}

int load16(void* dest, uint64_t len)
{
    int count = 0;
    for(uint64_t i = 0; i < len / 16; ++i) {
        count += *(int*)((uint64_t)dest + ((i * 16) << 6));
        count += *(int*)((uint64_t)dest + ((i * 16 + 1) << 6));
        count += *(int*)((uint64_t)dest + ((i * 16 + 2) << 6));
        count += *(int*)((uint64_t)dest + ((i * 16 + 3) << 6));
        count += *(int*)((uint64_t)dest + ((i * 16 + 4) << 6));
        count += *(int*)((uint64_t)dest + ((i * 16 + 5) << 6));
        count += *(int*)((uint64_t)dest + ((i * 16 + 6) << 6));
        count += *(int*)((uint64_t)dest + ((i * 16 + 7) << 6));
        count += *(int*)((uint64_t)dest + ((i * 16 + 8) << 6));
        count += *(int*)((uint64_t)dest + ((i * 16 + 9) << 6));
        count += *(int*)((uint64_t)dest + ((i * 16 + 10) << 6));
        count += *(int*)((uint64_t)dest + ((i * 16 + 11) << 6));
        count += *(int*)((uint64_t)dest + ((i * 16 + 12) << 6));
        count += *(int*)((uint64_t)dest + ((i * 16 + 13) << 6));
        count += *(int*)((uint64_t)dest + ((i * 16 + 14) << 6));
        count += *(int*)((uint64_t)dest + ((i * 16 + 15) << 6));
    }
    return count;
}
" > test_headers.h

sizes=(4 16) # 16 64 256 1024
for i in ${sizes[@]}; do
    echo ${i};
    echo "
    #include \"test_headers.h\"
    int main(int argc, char *argv[])
    {
        size_t size = (${i} << 6);
        int *test1, *orig;
        uint64_t offset = 0;
        orig = test1 = (int*)mmap(NULL, SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE, -1, 0);
        printf(\"%p\n\", test1);
        
        TEST_OP(clflushopt(test1, size));
        TEST_OP(clflushopt4(test1, size));
        TEST_OP(clflushopt16(test1, size));
        TEST_OP(clflush(test1, size));
        TEST_OP(clflush4(test1, size));
        TEST_OP(clflush16(test1, size));
        TEST_OP(store(test1, size));
        TEST_OP(store4(test1, size));
        TEST_OP(store16(test1, size));
        TEST_OP(load(test1, size));
        TEST_OP(load4(test1, size));
        TEST_OP(load16(test1, size));
        TEST_OP(clwb16(test1, size));
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

#./test_all_16

# All done!
m5 exit
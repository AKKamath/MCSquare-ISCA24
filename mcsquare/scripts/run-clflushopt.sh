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
#define PAGE_BITS 12
#define CL_BITS 6

#define TEST_OP(OPERATION) \
    m5_reset_stats(0, 0); \
    OPERATION; \
    _mm_mfence(); \
    m5_dump_stats(0, 0); \

void clflush(void* dest, uint64_t len)
{
    for(uint64_t i = 0; i < len; ++i) {
        _mm_clflushopt( (void*)((uint64_t)dest + (1 << 6)) );
    }
}

void clflushr(void* dest, uint64_t len)
{
    for(uint64_t i = 0; i < len; ++i) {
        _mm_clflush( (void*)((uint64_t)dest + (1 << 6)) );
    }
}

void store(void* dest, uint64_t len)
{
    for(uint64_t i = 0; i < len; ++i) {
        *(int*)((uint64_t)dest + (1 << 6)) = 0;
    }
}

void clflush4(void* dest, uint64_t len)
{
    for(uint64_t i = 0; i < len / 4; ++i) {
        _mm_clflushopt( (void*)((uint64_t)dest + (1 << 6)) );
        _mm_clflushopt( (void*)((uint64_t)dest + (1 << 6)) );
        _mm_clflushopt( (void*)((uint64_t)dest + (1 << 6)) );
        _mm_clflushopt( (void*)((uint64_t)dest + (1 << 6)) );
    }
}

void clflushr4(void* dest, uint64_t len)
{
    for(uint64_t i = 0; i < len / 4; ++i) {
        _mm_clflush( (void*)((uint64_t)dest + (1 << 6)) );
        _mm_clflush( (void*)((uint64_t)dest + (1 << 6)) );
        _mm_clflush( (void*)((uint64_t)dest + (1 << 6)) );
        _mm_clflush( (void*)((uint64_t)dest + (1 << 6)) );
    }
}

void store4(void* dest, uint64_t len)
{
    for(uint64_t i = 0; i < len / 4; ++i) {
        *(int*)((uint64_t)dest + (1 << 6)) = 0;
        *(int*)((uint64_t)dest + (1 << 6)) = 0;
        *(int*)((uint64_t)dest + (1 << 6)) = 0;
        *(int*)((uint64_t)dest + (1 << 6)) = 0;
    }
}

void clflush16(void* dest, uint64_t len)
{
    for(uint64_t i = 0; i < len / 16; ++i) {
        _mm_clflushopt( (void*)((uint64_t)dest + (1 << 6)) );
        _mm_clflushopt( (void*)((uint64_t)dest + (1 << 6)) );
        _mm_clflushopt( (void*)((uint64_t)dest + (1 << 6)) );
        _mm_clflushopt( (void*)((uint64_t)dest + (1 << 6)) );
        _mm_clflushopt( (void*)((uint64_t)dest + (1 << 6)) );
        _mm_clflushopt( (void*)((uint64_t)dest + (1 << 6)) );
        _mm_clflushopt( (void*)((uint64_t)dest + (1 << 6)) );
        _mm_clflushopt( (void*)((uint64_t)dest + (1 << 6)) );
        _mm_clflushopt( (void*)((uint64_t)dest + (1 << 6)) );
        _mm_clflushopt( (void*)((uint64_t)dest + (1 << 6)) );
        _mm_clflushopt( (void*)((uint64_t)dest + (1 << 6)) );
        _mm_clflushopt( (void*)((uint64_t)dest + (1 << 6)) );
        _mm_clflushopt( (void*)((uint64_t)dest + (1 << 6)) );
        _mm_clflushopt( (void*)((uint64_t)dest + (1 << 6)) );
        _mm_clflushopt( (void*)((uint64_t)dest + (1 << 6)) );
        _mm_clflushopt( (void*)((uint64_t)dest + (1 << 6)) );
    }
}

void clwb16(void* dest, uint64_t len)
{
    for(uint64_t i = 0; i < len / 16; ++i) {
        _mm_clwb( (void*)((uint64_t)dest + (1 << 6)) );
        _mm_clwb( (void*)((uint64_t)dest + (1 << 6)) );
        _mm_clwb( (void*)((uint64_t)dest + (1 << 6)) );
        _mm_clwb( (void*)((uint64_t)dest + (1 << 6)) );
        _mm_clwb( (void*)((uint64_t)dest + (1 << 6)) );
        _mm_clwb( (void*)((uint64_t)dest + (1 << 6)) );
        _mm_clwb( (void*)((uint64_t)dest + (1 << 6)) );
        _mm_clwb( (void*)((uint64_t)dest + (1 << 6)) );
        _mm_clwb( (void*)((uint64_t)dest + (1 << 6)) );
        _mm_clwb( (void*)((uint64_t)dest + (1 << 6)) );
        _mm_clwb( (void*)((uint64_t)dest + (1 << 6)) );
        _mm_clwb( (void*)((uint64_t)dest + (1 << 6)) );
        _mm_clwb( (void*)((uint64_t)dest + (1 << 6)) );
        _mm_clwb( (void*)((uint64_t)dest + (1 << 6)) );
        _mm_clwb( (void*)((uint64_t)dest + (1 << 6)) );
        _mm_clwb( (void*)((uint64_t)dest + (1 << 6)) );
    }
}

void clflushr16(void* dest, uint64_t len)
{
    for(uint64_t i = 0; i < len / 16; ++i) {
        _mm_clflush( (void*)((uint64_t)dest + (1 << 6)) );
        _mm_clflush( (void*)((uint64_t)dest + (1 << 6)) );
        _mm_clflush( (void*)((uint64_t)dest + (1 << 6)) );
        _mm_clflush( (void*)((uint64_t)dest + (1 << 6)) );
        _mm_clflush( (void*)((uint64_t)dest + (1 << 6)) );
        _mm_clflush( (void*)((uint64_t)dest + (1 << 6)) );
        _mm_clflush( (void*)((uint64_t)dest + (1 << 6)) );
        _mm_clflush( (void*)((uint64_t)dest + (1 << 6)) );
        _mm_clflush( (void*)((uint64_t)dest + (1 << 6)) );
        _mm_clflush( (void*)((uint64_t)dest + (1 << 6)) );
        _mm_clflush( (void*)((uint64_t)dest + (1 << 6)) );
        _mm_clflush( (void*)((uint64_t)dest + (1 << 6)) );
        _mm_clflush( (void*)((uint64_t)dest + (1 << 6)) );
        _mm_clflush( (void*)((uint64_t)dest + (1 << 6)) );
        _mm_clflush( (void*)((uint64_t)dest + (1 << 6)) );
        _mm_clflush( (void*)((uint64_t)dest + (1 << 6)) );
    }
}

void store16(void* dest, uint64_t len)
{
    for(uint64_t i = 0; i < len / 16; ++i) {
        *(int*)((uint64_t)dest + (1 << 6)) = 0;
        *(int*)((uint64_t)dest + (1 << 6)) = 0;
        *(int*)((uint64_t)dest + (1 << 6)) = 0;
        *(int*)((uint64_t)dest + (1 << 6)) = 0;
        *(int*)((uint64_t)dest + (1 << 6)) = 0;
        *(int*)((uint64_t)dest + (1 << 6)) = 0;
        *(int*)((uint64_t)dest + (1 << 6)) = 0;
        *(int*)((uint64_t)dest + (1 << 6)) = 0;
        *(int*)((uint64_t)dest + (1 << 6)) = 0;
        *(int*)((uint64_t)dest + (1 << 6)) = 0;
        *(int*)((uint64_t)dest + (1 << 6)) = 0;
        *(int*)((uint64_t)dest + (1 << 6)) = 0;
        *(int*)((uint64_t)dest + (1 << 6)) = 0;
        *(int*)((uint64_t)dest + (1 << 6)) = 0;
        *(int*)((uint64_t)dest + (1 << 6)) = 0;
        *(int*)((uint64_t)dest + (1 << 6)) = 0;
    }
}
" > test_headers.h

sizes=(4 16 64 256 1024)
for i in ${sizes[@]}; do
    echo ${i};
    echo "
    #include \"test_headers.h\"
    int main(int argc, char *argv[])
    {
        size_t size;
        int test2;
        int *test1 = &test2;
        size = ${i};
        //test1 = (int*)mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE, -1, 0);
        printf(\"%p\n\", test1);
        TEST_OP(clflush(test1, size));
        //TEST_OP(clflush4(test1, size));
        //TEST_OP(clflush16(test1, size));
        //TEST_OP(clflushr(test1, size));
        //TEST_OP(clflushr4(test1, size));
        //TEST_OP(clflushr16(test1, size));
        //TEST_OP(clwb16(test1, size));
        TEST_OP(store(test1, size));
        //TEST_OP(store4(test1, size));
        //TEST_OP(store16(test1, size));
        return 0;
    }
    " > test_all_${i}.cpp;
    g++ test_all_$i.cpp -o test_all_$i -lrt -g -march=native -I../include ../util/m5/build/x86/out/libm5.a
done

#objdump -d test_all_4

echo "Done compilation"
m5 exit

# Begin tests
#for i in ${sizes[@]}; do
#    echo "Test: size $i"
#    ./test_all_$i
#done

./test_all_16

# All done!
m5 exit
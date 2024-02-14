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
#define SIZE (4096*1024)
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
    verify(dest, src, size, accesses);

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
    TEST_OP(memcpy, test2, test1, size / 64, ACCESSES / 64);
    TEST_OP(memcpy, test2, test1, size / 16, ACCESSES / 16);
    TEST_OP(memcpy, test2, test1, size / 4, ACCESSES / 4);
    TEST_OP(memcpy, test2, test1, size, ACCESSES);
    return 0;
}
" > test_memcpy.cpp;

tests="test_memcpy"
for i in $tests; do
    g++ $i.cpp -o $i -lrt -g -march=native -I../include ../util/m5/build/x86/out/libm5.a
done

echo "Done compilation"
m5 exit

for i in $tests; do
    ./$i
done
m5 exit
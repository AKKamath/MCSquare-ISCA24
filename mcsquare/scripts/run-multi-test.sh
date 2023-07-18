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
    reset_op(test2, test1, size); \
    _mm_mfence();  \
    m5_reset_stats(0, 0); \
    OPERATION; \
    _mm_mfence(); \
    m5_dump_stats(0, 0); \
    printf(\"Dest: %d Src: %d\n\", *test2, *test1); \
    fflush(stdout); \
    memcpy_elide_free(test2, size);

void reset_op(int* dest, int* src, uint64_t size) {
    for(int i = 0; i < size / sizeof(int); i += PAGE_SIZE / sizeof(int)) {
        src[i]  = 500;
        dest[i] = 100;
    }
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
    uint64_t flush_sz = len < PAGE_SIZE ? (len + 63) / 64 : 64;
    for(uint64_t page = 0; page < pages; ++page) {
        for (uint64_t i = 0; i < flush_sz; ++i) {
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
    uint64_t flush_sz = len < PAGE_SIZE ? (len + 63) / 64 : 64;
    uint64_t pages = (len >> PAGE_BITS) + (len & ((1 << PAGE_BITS) - 1) ? 1 : 0);
    for(uint64_t page = 0; page < pages; ++page) {
        for (uint64_t i = 0; i < flush_sz; ++i) {
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

sizes=(64 256 1024 4096 16384 65536 262144 1048576)
for i in ${sizes[@]}; do
    echo ${i};
    echo "
    #include \"test_headers.h\"
    int main(int argc, char *argv[])
    {
        size_t size;
        int *test1, *test2;
        size = ${i};
        test1 = (int*)mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE, -1, 0);
        test2 = (int*)mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE, -1, 0);
        printf(\"%p\n\", test1);
        printf(\"%p\n\", test2);
        TEST_OP(memcpy_elide_pgflush(test2, test1, size));

        printf(\"%p\n\", test1);
        printf(\"%p\n\", test2);
        TEST_OP(memcpy_elide_clflush(test2, test1, size));

        printf(\"%p\n\", test1);
        printf(\"%p\n\", test2);
        TEST_OP(memcpy_elide_clflush_src(test2, test1, size));

        printf(\"%p\n\", test1);
        printf(\"%p\n\", test2);
        TEST_OP(memcpy(test2, test1, size));
        return 0;
    }
    " > test_all_${i}.cpp;
    g++ test_all_$i.cpp -o test_all_$i -lrt -g -march=native -I../include ../util/m5/build/x86/out/libm5.a
    
    echo "
    #include \"test_headers.h\"
    int main(int argc, char *argv[])
    {
        size_t size;
        int *test1, *test2;
        size = ${i};
        test1 = (int*)mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE, -1, 0);
        test2 = (int*)mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE, -1, 0);
        printf(\"%p\n\", test1);
        printf(\"%p\n\", test2);
        TEST_OP(memcpy(test2, test1, size));
        return 0;
    }
    " > test_memcpy_${i}.cpp;
    g++ test_memcpy_$i.cpp -o test_memcpy_$i -lrt -g -march=native -I../include ../util/m5/build/x86/out/libm5.a
done


# Compile zIO
ZIO=/home/akkamath/zIO
ZIO_BIN=${ZIO}/copy_interpose.so
pushd ${ZIO};
make
popd;

echo "Done compilation"
sleep 2
m5 exit

# Begin tests
for i in ${sizes[@]}; do
    echo "Test: size $i"
    ./test_all_$i
    LD_PRELOAD=${ZIO_BIN} ./test_memcpy_$i
done

# All done!
m5 exit
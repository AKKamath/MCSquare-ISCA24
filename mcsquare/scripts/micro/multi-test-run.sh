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
#define CL_SIZE 64
#define PAGE_BITS 12
#define CL_BITS 6

#define cust_min(a, b) (((a) < (b)) ? (a) : (b))
#define MCLAZY(dest, src, size) \
        asm volatile(\".byte 0x0F, 0x0A\" : : \"D\"(dest), \"S\"(src), \"d\"(size));

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
        _mm_clflush(&src[i]);
        _mm_clflush(&dest[i]);
    }
}

void memcpy_elide_pgflush(void* dest, void* src, uint64_t len)
{
    uint64_t temp_src = ((uint64_t)src & ~((uint64_t)63));
    while(temp_src < (uint64_t)src + len) {
        _mm_clwb( (void*)temp_src );
        temp_src += PAGE_SIZE;
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
        MCLAZY(dest, src, elide_size);
        dest = (void *)((char *)dest + elide_size);
        src = (void *)((char *)src + elide_size);
        len -= elide_size;
    }
}

static void memcpy_elide_clwb(void* dest, const void* src, uint64_t len)
{
    uint64_t temp_src = ((uint64_t)src & ~((uint64_t)63));
    while(temp_src < (uint64_t)src + len) {
        _mm_clwb( (void*)temp_src );
        temp_src += CL_SIZE;
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
          MCLAZY(dest, (void*)temp_src, elide_size);
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

sizes=(64 256 1024 4096 16384 65536 262144 1048576 4194304)
for i in ${sizes[@]}; do
    echo ${i};
    echo "
    #include \"test_headers.h\"
    int main(int argc, char *argv[])
    {
        size_t size;
        int *test1, *test2;
        size = ${i};
        test1 = (int*)mmap(NULL, size + 16, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE, -1, 0);
        test2 = (int*)mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE, -1, 0);
        printf(\"%p\n\", test1);
        printf(\"%p\n\", test2);
        TEST_OP(memcpy_elide_clwb(test2, test1 + 16 / sizeof(int), size));

        //printf(\"%p\n\", test1);
        //printf(\"%p\n\", test2);
        //TEST_OP(memcpy_elide_clwb(test2, test1, size));

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
        recv(-2, NULL, 0, 0);
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
#cd /home/akkamath/gem5-zIO/util/m5
#scons build/x86/out
cd /home/akkamath/gem5-zIO/mcsquare
ls
for i in 1024 4096; do
    COPY_SIZE=${i}
    echo "
    //#include <gem5/m5ops.h>
    #include <sys/mman.h>
    #include <sys/stat.h>
    #include <sys/socket.h>
    #include <fcntl.h>
    #include <unistd.h>
    #include <stdio.h>
    #include <stdlib.h>
    #include <x86intrin.h>
    #include <stdint.h>
    #include <string.h>
    #include <sys/syscall.h>         /* Definition of SYS_* constants */
    #include <sys/socket.h>
    #include <string.h>
    #define SIZE (4096 * 32)
    #define COPY_SIZE ${COPY_SIZE}
    #define TRANSACTIONS 2000
    #define PAGE_SIZE 4096
    #define PAGE_BITS 12
    #define CL_SIZE 64
    #define CL_BITS 6
    #define ACCESSES (COPY_SIZE / sizeof(uint64_t))

    #define cust_min(a, b) (((a) < (b)) ? (a) : (b))
    #define MCLAZY(dest, src, size) \
            asm volatile(\".byte 0x0F, 0x0A\" : : \"D\"(dest), \"S\"(src), \"d\"(size));

    #define TIME_NOW __rdtsc()
    #define TIME_DIFF(a, b) (a - b)
    uint64_t time_start, time_stop;

    //    m5_reset_stats(0,0); \
        //m5_dump_stats(0,0); \

    // This function generates a random number using an LFSR.
    static uint64_t lfsr(uint64_t lfsr)
    {
        lfsr ^= lfsr >> 7;
        lfsr ^= lfsr << 9;
        lfsr ^= lfsr >> 13;
        return lfsr;
    }

    #define TEST_OP(COPY_OP, WRITE_OP, dest, src, size, accesses) \
        time_start = TIME_NOW; \
        for(int i = 0; i < TRANSACTIONS; ++i) {\
            COPY_OP(dest, &src[(lfsr(i) % (size / COPY_SIZE)) * COPY_SIZE / sizeof(src[0])], COPY_SIZE); \
            WRITE_OP(dest, &src[(lfsr(i) % (size / COPY_SIZE)) * COPY_SIZE / sizeof(src[0])], COPY_SIZE, accesses); \
        } \
        time_stop = TIME_NOW; \
        printf(\"time: %lu\n\", TIME_DIFF(time_stop, time_start)); \
        memcpy_elide_free(dest, size);

    void reset_op(uint64_t* dest, uint64_t* src, uint64_t size) {
        for(int i = 0; i < size / sizeof(uint64_t); i += PAGE_SIZE / sizeof(uint64_t)) {
            src[i]  = 500;
            dest[i] = 100;
        }
    }

    void store_test(uint64_t* dest, uint64_t* src, uint64_t size, uint64_t accesses) {
        //auto start = TIME_NOW;
        uint64_t verify = 0;
        for(int i = 0; i < accesses; i++) {
            dest[i] = 0;
        }
        //auto stop = TIME_NOW;
        //printf(\"Verify: %lu\n\", verify);
    }

    void ntstore_test(uint64_t* dest, uint64_t* src, uint64_t size, uint64_t accesses) {
        //auto start = TIME_NOW;
        uint64_t verify = 0;
        for(int i = 0; i < accesses; i++) {
            _mm_stream_si64((long long int*)&dest[i], 0);
        }
        //auto stop = TIME_NOW;
        //printf(\"Verify: %lu\n\", verify);
    }

    void ntstorebig_test(uint64_t* dest, uint64_t* src, uint64_t size, uint64_t accesses) {
        //auto start = TIME_NOW;
        uint64_t verify = 0;
        for(int i = 0; i < accesses; i += 2) {
            _mm_stream_si128 ((__m128i*)&dest[i], __m128i());
        }
        //auto stop = TIME_NOW;
        //printf(\"Verify: %lu\n\", verify);
    }

    void memcpy_elide_pgflush(void* dest, void* src, uint64_t len)
    {
        void *temp_dest = (void*)((uint64_t)dest & ~((uint64_t)63));
        void *temp_src = (void*)((uint64_t)src & ~((uint64_t)63));
        uint64_t pages = (len >> PAGE_BITS) + (len & ((1 << PAGE_BITS) - 1) ? 1 : 0);
        for (uint64_t page = 0; page < pages; ++page) {
            _mm_clwb( (void*)((uint64_t)temp_src + (page << PAGE_BITS)) );
            _mm_mfence();
            MCLAZY((void*)((uint64_t)temp_dest + (page << PAGE_BITS)),
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
        /*uint64_t temp_dest = ((uint64_t)dest & ~((uint64_t)4095));
        while(temp_dest < (uint64_t)dest + len) {
            _mm_clwb( (void*)temp_dest );
            temp_dest += PAGE_SIZE;
        }*/
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
        //m5_memcpy_elide_free(dest, 1);
        _mm_mfence();
    }
    " > test_headers_${COPY_SIZE}.h
    echo "
    #include <pthread.h> 
    #include \"test_headers_${COPY_SIZE}.h\"
    #define THREADS 4

    uint64_t *test1, *test2;
    size_t size;
    void* myThreadFun(void *vargp) {
        uint64_t time_starter = TIME_NOW;
        for(int i = 0; i < TRANSACTIONS; ++i) {
            uint64_t index = lfsr(i);
            for(int j = 0; j < COPY_SIZE / sizeof(test2[0]); ++j) {
                test2[j + (index % (size / COPY_SIZE)) * COPY_SIZE / sizeof(test2[0])] = 0;
            }
            memcpy_elide_clwb(&test2[(index % (size / COPY_SIZE)) * COPY_SIZE / sizeof(test2[0])], test1, COPY_SIZE);
            //COPY_OP(dest, &src[(lfsr(i) % (size / COPY_SIZE)) * COPY_SIZE / sizeof(src[0])], COPY_SIZE);
            //WRITE_OP(dest, &src[(lfsr(i) % (size / COPY_SIZE)) * COPY_SIZE / sizeof(src[0])], COPY_SIZE, accesses);
        }
        uint64_t time_stopper = TIME_NOW;
        printf(\"time: %lu\n\", TIME_DIFF(time_stopper, time_starter));
        return NULL;
    }
    
    void* myThreadFun2(void *vargp) {
        uint64_t tid = *((uint64_t *)vargp);
        uint64_t time_starter = TIME_NOW;
        uint64_t sum = 0;
        for(int i = 0; i < TRANSACTIONS; ++i) {
            uint64_t index = lfsr(i);
            for(int j = 0; j < COPY_SIZE / sizeof(test2[0]); ++j) {
                sum += test2[j + (index % (size / COPY_SIZE)) * COPY_SIZE / sizeof(test2[0])];
            }
            //COPY_OP(dest, &src[(lfsr(i) % (size / COPY_SIZE)) * COPY_SIZE / sizeof(src[0])], COPY_SIZE);
            //WRITE_OP(dest, &src[(lfsr(i) % (size / COPY_SIZE)) * COPY_SIZE / sizeof(src[0])], COPY_SIZE, accesses);
        }
        uint64_t time_stopper = TIME_NOW;
        printf(\"time: %lu\n\", TIME_DIFF(time_stopper, time_starter));
        return NULL;
    }

    int main(int argc, char *argv[])
    {
        size = SIZE;
        test1 = (uint64_t*)aligned_alloc(PAGE_SIZE, size + PAGE_SIZE);
        test2 = (uint64_t*)aligned_alloc(PAGE_SIZE, size + PAGE_SIZE);
        test1 = &test1[128];
        test2 = &test2[2];
        printf(\"%p\n\", test1);
        printf(\"%p\n\", test2);
        printf(\"Copy size: %u\n\", COPY_SIZE);
        reset_op(test2, test1, size);
        printf(\"Dest: %lu Src: %lu\n\", *test2, *test1);
        _mm_mfence();

        pthread_t tid[THREADS]; 
        uint64_t *threadID = (uint64_t *)calloc(THREADS, sizeof(uint64_t));
        for(int i = 0; i < THREADS; ++i)
            threadID[i] = i;
        printf(\"Before Thread\n\"); 
        for(int i = 0; i < THREADS; i++)
            if(i % 2)
                pthread_create(&tid[i], NULL, myThreadFun, (void *)&threadID[i]); 
            else
                pthread_create(&tid[i], NULL, myThreadFun2, (void *)&threadID[i]); 
        for(int i = 0; i < THREADS; i++)
            pthread_join(tid[i], NULL); 
        printf(\"After Thread\n\"); 
        return 0;
    }
    " > test_clwb_align_${COPY_SIZE}.cpp;
done

tests="test_clwb_align_1024 test_clwb_align_4096"
for i in $tests; do
    g++ $i.cpp -o $i -lrt -g -march=native -lpthread #-I../include ../util/m5/build/x86/out/libm5.a
done

echo "Done compilation"
m5 exit

for i in $tests; do
    ./$i
done
m5 exit
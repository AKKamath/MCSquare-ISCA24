

echo never | tee /sys/kernel/mm/transparent_hugepage/enabled

cd /home/akkamath/gem5-zIO/util/m5
scons build/x86/out
cd /home/akkamath/gem5-zIO/mcsquare

echo "

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <x86intrin.h>
#include <stdint.h>
#include <gem5/m5ops.h>
#include \"mcsquare.h\"
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>

#define ITERS 200
#define TIME_NOW __rdtsc()
#define TIME_DIFF(a, b) (a - b)

int    pipefd[2];
char   buf;
pid_t  cpid;
double accesses;
uint64_t xfer_size;
int log_threads;
#define MCFREE(a, b) asm volatile(\".byte 0x0F, 0x0C\" : : \"D\"(a), \"d\"(b)); asm volatile(\";\");

uint64_t start, end;

void send_pipe() {
    int *arr = (int*)mmap(NULL, xfer_size * ITERS, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE, -1, 0);
    memset(arr, 0, xfer_size * ITERS);
    //printf(\"Sending pipe %p\n\", arr);
    start = TIME_NOW;
    for(int j = 0; j < ITERS; ++j) {
        arr[xfer_size / sizeof(int) * j] = 1;
        write(pipefd[1], &arr[xfer_size / sizeof(int) * j], xfer_size);
    }
    end = TIME_NOW;
    printf(\"%.2f: Write pipe: %ld cycles\n\", accesses, TIME_DIFF(end, start));
    close(pipefd[1]);          /* Reader will see EOF */
}

void recv_pipe(double accesses) {
    int *arr = (int*)mmap(NULL, xfer_size * ITERS, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE, -1, 0);
    int data = 0;
    //printf(\"Reading pipe %p\n\", arr);
    int j = 0;
    start = TIME_NOW;
    uint64_t read_time = 0, acc_time = 0;

    for(int j = 0; j < ITERS; ++j) {
        uint64_t start2 = TIME_NOW;
        if(read(pipefd[0], &arr[xfer_size / sizeof(int) * j], xfer_size) > 0) {
            uint64_t end2 = TIME_NOW;
            read_time += TIME_DIFF(end2, start2);
            //memcpy_elide_free(&arr[xfer_size / sizeof(int) * j], xfer_size);
        }
    }
    end = TIME_NOW;
    printf(\"%d threads, %lu size; %.2f: Read pipe: %ld cycles. Read %ld, acc %ld; Data %d\n\", log_threads, xfer_size, accesses, 
        read_time + acc_time, read_time, acc_time, data);
    close(pipefd[0]);
}

int main(int argc, char *argv[]) {
    // Get arguments
    accesses = strtod(argv[1], NULL);
    xfer_size = atoi(argv[2]);
    log_threads = atoi(argv[3]);

    // Create split of threads
    for(int i = 0; i < log_threads; ++i) {
        cpid = fork();
        if (cpid == -1) {
            perror(\"fork\");
            exit(EXIT_FAILURE);
        }
    }

    start = TIME_NOW;
    if (pipe(pipefd) == -1) {
        perror(\"pipe\");
        exit(EXIT_FAILURE);
    }

    int pipe_sz = fcntl(pipefd[1], F_SETPIPE_SZ, 1048576);

    end = TIME_NOW;
    printf(\"Create pipe: %ld cycles. Size %d\n\", TIME_DIFF(end, start), pipe_sz);

    cpid = fork();
    if (cpid == -1) {
        perror(\"fork\");
        exit(EXIT_FAILURE);
    }

    if (cpid == 0) {    /* Child reads from pipe */
        recv_pipe(accesses);
        m5_memcpy_elide_free(&start, 1);
    } else {
        send_pipe();
        wait(NULL);                /* Wait for child */
    }
}
" > pipe_test.c

gcc pipe_test.c -o pipe_test -march=native -I/home/akkamath/gem5-zIO/mcsquare/lib -I/home/akkamath/gem5-zIO/include /home/akkamath/gem5-zIO/util/m5/build/x86/out/libm5.a
echo "run
bt" > run.sh
chmod +x run.sh
echo "Done compilation"
sleep 2
m5 exit

SIZES="1021"
THREADS="0"

#for j in 1 0.5 0.25 0; do
    for xfer in ${SIZES}; do
        for log_threads in ${THREADS}; do
            printf "Running xfer: %d log_threads: %d\n" $xfer $log_threads
            m5 resetstats
            ./pipe_test 0 $xfer $log_threads || true
            m5 dumpstats
        done
    done
#done

# All done!
m5 exit

#gdb -x run.sh --args 
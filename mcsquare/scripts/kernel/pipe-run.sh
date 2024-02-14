

echo never | tee /sys/kernel/mm/transparent_hugepage/enabled

cd /home/akkamath/gem5-zIO/util/m5
scons build/x86/out
cd /home/akkamath/gem5-zIO/mcsquare

echo "#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <x86intrin.h>
#include <stdint.h>
#include <gem5/m5ops.h>
#include <sys/mman.h>
#include <sys/stat.h>

int    pipefd[2];
char   buf;
pid_t  cpid;
#define SIZE 65536
#define ITERS 100
#define TIME_NOW __rdtsc()
#define TIME_DIFF(a, b) (a - b)
double accesses;

uint64_t start, end;

void send_pipe() {
    int *arr = (int*)mmap(NULL, SIZE * ITERS, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE, -1, 0);
    memset(arr, 0, SIZE * ITERS);
    printf(\"Sending pipe %p\n\", arr);
    start = TIME_NOW;
    for(int j = 0; j < ITERS; ++j) {
        arr[SIZE / sizeof(int) * j] = 1;
        write(pipefd[1], &arr[SIZE / sizeof(int) * j], SIZE);
    }
    end = TIME_NOW;
    printf(\"%.2f: Write pipe: %ld cycles\n\", accesses, TIME_DIFF(end, start));
    close(pipefd[1]);          /* Reader will see EOF */
}

void recv_pipe(double accesses) {
    int *arr = (int*)mmap(NULL, SIZE * ITERS, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE, -1, 0);
    int data = 0;
    printf(\"Reading pipe %p\n\", arr);
    int j = 0;
    start = TIME_NOW;
    uint64_t read_time = 0, acc_time = 0;

    for(int j = 0; j < ITERS; ++j) {
        uint64_t start2 = TIME_NOW;
        if(read(pipefd[0], &arr[SIZE / sizeof(int) * j], SIZE) > 0) {
            uint64_t end2 = TIME_NOW;
            read_time += TIME_DIFF(end2, start2);
            start2 = TIME_NOW;
            for(int i = 0; i < SIZE * accesses / sizeof(int); ++i) {
                data += arr[SIZE / sizeof(int) * j + i];
                //printf(\"%x %d\n\", &arr[SIZE / sizeof(int) * j + i], data);
            }
            acc_time += TIME_DIFF(TIME_NOW, start2);
        }
    }
    end = TIME_NOW;
    printf(\"%.2f: Read pipe: %ld cycles. Read %ld, acc %ld; Data %d\n\", accesses, 
        TIME_DIFF(end, start), read_time, acc_time, data);
    close(pipefd[0]);
}

int main(int argc, char *argv[]) {
    start = TIME_NOW;
    if (pipe(pipefd) == -1) {
        perror(\"pipe\");
        exit(EXIT_FAILURE);
    }
    end = TIME_NOW;
    printf(\"Create pipe: %ld cycles\n\", TIME_DIFF(end, start));

    accesses = strtod(argv[1], NULL);
    cpid = fork();
    if (cpid == -1) {
        perror(\"fork\");
        exit(EXIT_FAILURE);
    }

    if (cpid == 0) {    /* Child reads from pipe */
        recv_pipe(accesses);
    } else {
        send_pipe();
        wait(NULL);                /* Wait for child */
    }
    m5_memcpy_elide_free(&start, 1);
}
" > pipe_test.c

gcc pipe_test.c -o pipe_test -I/home/akkamath/gem5-zIO/include /home/akkamath/gem5-zIO/util/m5/build/x86/out/libm5.a

echo "Done compilation"
sleep 2
m5 exit

for j in 1 0.5 0.25 0; do
    for i in {1..3}; do
        numactl --physcpubind=0,1 ./pipe_test $j
    done
done

# All done!
m5 exit
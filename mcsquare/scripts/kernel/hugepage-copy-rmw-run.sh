
echo "#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/wait.h>
#include <x86intrin.h>
#include <stdint.h>
#include <sys/mman.h> // Add missing import

#define TIME_NOW __rdtsc()
#define TIME_DIFF(a, b) (a - b)

typedef uint64_t Type;

void init_arr(int *arr, size_t elems) {
    for(size_t i = 0; i < elems; ++i)
        arr[i] = i;
}

// This function generates a random number using an LFSR.
static uint64_t lfsr(uint64_t lfsr)
{
  lfsr ^= lfsr >> 7;
  lfsr ^= lfsr << 9;
  lfsr ^= lfsr >> 13;
  return lfsr;
}

void perform_accesses(Type *arr, size_t elems, size_t num_access) {
#define ITERS 100
    uint64_t start = TIME_NOW, end;
    size_t index = 1;
    for(int j = 0; j < ITERS; ++j) {
        for(size_t i = 0; i < num_access / ITERS; ++i) {
            index = lfsr(index) % elems;
            arr[index] += -1;
        }
        end = TIME_NOW;
        printf(\"Iter %d: \t%ld cycles\n\", j, TIME_DIFF(end, start));
        fflush(stdout);
    }
}

int main(int argc, char *argv[]) {
    if(argc < 3) {
        fprintf(stderr, \"Format: %s ARR_SIZE NUM_ACCESS\n\", argv[0]);
        return -1;
    }
    printf(\"Started benchmark\n\");

    const size_t ARR_SIZE = atoi(argv[1]);
    const size_t NUM_ACCESS = atoi(argv[2]);

    Type *arr = (Type *)mmap(NULL, ARR_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB | MAP_POPULATE, -1, 0);
    if (arr == MAP_FAILED) { // Check if mmap was successful
        perror(\"mmap failed\");
        return -1; // Handle the error appropriately
    }
    printf(\"Map complete\n\");

    //init_arr(arr, ARR_SIZE / sizeof(Type));
    printf(\"Init complete\n\");

    int pid = 0;
    pid = fork();

    if(pid == 0) {
        // Child does accesses
        perform_accesses(arr, ARR_SIZE / sizeof(Type), NUM_ACCESS);
    } else {
        int wstatus;
        // Parent waits for child to die
        waitpid(-1, &wstatus, 0);
    }
}
" > copy_test.c

gcc copy_test.c -o copy_test

echo "100" > /proc/sys/vm/nr_hugepages

echo "Done compilation"
sleep 2
m5 exit

./copy_test $((64 * 1024 * 1024)) 2097152

# All done!
m5 exit
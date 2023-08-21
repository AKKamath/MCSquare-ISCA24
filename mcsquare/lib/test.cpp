#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <cstdint>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#define MAX_SIZE (4096 * 4)

int main() {
    void *dest = malloc(MAX_SIZE);
    void *src = malloc(MAX_SIZE);
    //dest = (void *)(((uint64_t)dest + 64) & ~(63));
    //src = (void *)(((uint64_t)src + 64) & ~(63));
    printf("%p %p\n", dest, src);
    free(src);
    free(dest);
    dest = malloc(MAX_SIZE);
    src = malloc(MAX_SIZE);
    for(int size = 4096; size <= MAX_SIZE; size *= 2) {
        memcpy(dest, src, size);
        printf("%p %p %d\n", dest, src, size);
    }
    void *addr = mmap(NULL, MAX_SIZE, PROT_READ,
                MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    munmap(addr, MAX_SIZE);
    return 0;
}
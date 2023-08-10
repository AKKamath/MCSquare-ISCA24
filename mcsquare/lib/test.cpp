#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <cstdint>
#define MAX_SIZE (4096 * 4096)

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
    for(int size = 32; size < MAX_SIZE; size *= 2)
        memcpy(dest, src, size);
    return 0;
}
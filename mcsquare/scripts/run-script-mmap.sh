cd /home/akkamath/gem5-zIO/util/m5
scons build/x86/out
cd /home/akkamath/gem5-zIO/mcsquare
ls
echo "
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define SIZE (4096)
#define PAGE_SIZE 4096
#define PAGE_BITS 12
#define CL_BITS 6

int main(int argc, char *argv[])
{   
    int *test = (int*)mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, -1, 0);
    printf("%p\n", test);
    test[0] = 100;
    return 0;
}
" > test_file.cpp;
#echo "run; bt; quit;" > run.sh
g++ test_file.cpp -o test_file -lrt -g
echo "HERE"
objdump -d test_file
m5 exit
./test_file
m5 exit
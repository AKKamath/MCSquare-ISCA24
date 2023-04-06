cd /home/akkamath/gem5-zIO/util/m5
scons build/x86/out
cd /home/akkamath/gem5-zIO/zio_stuff
ls
#make copy_sweep
#./copy_sweep 4 4096 4
#g++ copy_sweep2.cpp -o copy_sweep2 -lrt -g  -I../include ../util/m5/build/x86/out/libm5.a -lpthread -pthread
#./copy_sweep2

echo "
#include <gem5/m5ops.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[])
{
    int *test1 = 0, *test2 = 0;
    size_t size = 4096;
    test1 = (int*)malloc(size);
    test2 = (int*)malloc(size);
    printf(\"%p\n\", test1);
    printf(\"%p\n\", test2);
    m5_memcpy_elide(test1, test2, size);
    printf(\"%d\n\", *test2);
    return 0;
}
" > test_file.cpp;
echo "run; bt; quit;" > run.sh
g++ test_file.cpp -o test_file -lrt -g  -I../include ../util/m5/build/x86/out/libm5.a
sleep 5
m5 exit
echo "HERE"
ls
./test_file
m5 exit
#include <fcntl.h>
#include <linux/hw_breakpoint.h>
#include <linux/perf_event.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/syscall.h>         /* Definition of SYS_* constants */
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <gem5/m5ops.h>

int main(int argc, char *argv[])
{
    int *test1 = 0, *test2 = 0;
    size_t test3 = 100;
    m5_switch_cpu();
    m5_memcpy_elide(test1, test2, test3);
    return 0;
}



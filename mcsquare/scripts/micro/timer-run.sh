echo "
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <chrono>

int main(int argc, char **argv)
{
    timespec tp, tp2;
    clock_getres(CLOCK_REALTIME, &tp);
    printf(\"%ld %ld\n\", tp.tv_sec, tp.tv_nsec);
    clock_gettime(CLOCK_REALTIME, &tp);
    for(int i = 0; i < 10000; i++) {
      clock_gettime(CLOCK_REALTIME, &tp2);
    }
    printf(\"%ld %ld\n\", tp2.tv_sec - tp.tv_sec, tp2.tv_nsec - tp.tv_nsec);

    timeval tv, tv2;
    gettimeofday(&tv, 0);
    for(int i = 0; i < 10000; i++) {
      gettimeofday(&tv2, 0);
    }
    printf(\"%ld %ld\n\", tv2.tv_sec - tv.tv_sec, tv2.tv_usec - tv.tv_usec);
/*
#elif defined(_GLIBCXX_USE_GETTIMEOFDAY)
      timeval tv;
      // EINVAL, EFAULT
      gettimeofday(&tv, 0);
      return time_point(duration(chrono::seconds(tv.tv_sec)
				 + chrono::microseconds(tv.tv_usec)));
#else
      std::time_t __sec = std::time(0);
      return system_clock::from_time_t(__sec);
#endif*/
    return 0;
}

" > timer.cpp

g++ timer.cpp -o timer

m5 exit

cat /sys/devices/system/clocksource/clocksource0/available_clocksource
cat /sys/devices/system/clocksource/clocksource0/current_clocksource

./timer
m5 exit
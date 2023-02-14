#include <stdio.h>
#include <stdlib.h>
#include <chrono>
#include <pthread.h>
#include <string.h>
#include <sys/socket.h>
#include <linux/perf_event.h>
#include <linux/hw_breakpoint.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>         /* Definition of SYS_* constants */
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <gem5/m5ops.h>

using namespace std::chrono;

#define MAX_NUM_COPYS 128
#define MAX_THREADS 16
#define PAGE_SIZE 4096
#define MAX_ENTRIES 4096

#define TIME_NOW high_resolution_clock::now()
#define TIME_DIFF(a, b) duration_cast<milliseconds>(a - b).count()

static uint64_t max_bytes = 2048;
static uint64_t max_time = 0;

static long perf_event_open(struct perf_event_attr *hw_event,
                pid_t pid,
                int cpu,
                int group_fd,
                unsigned long flags) {
  int ret = syscall(__NR_perf_event_open, hw_event, pid, cpu,
            group_fd, flags);
  return ret;
}

struct fifo_buffer {
    void *buffer[MAX_ENTRIES];
    void *buffer_copy[MAX_ENTRIES];
    pthread_mutex_t mutex;
    int head;
    int tail;
    fifo_buffer() {head = -1; tail = -1;}
    void init();
    bool pop(void **a, void **b);
    bool push(void *a, void *b);
};

void fifo_buffer::init() {
    head = -1;
    tail = -1;
    pthread_mutex_init(&mutex, NULL);
}

bool fifo_buffer::pop(void **a, void **b) {
    pthread_mutex_lock(&mutex);
    if(head == -1) {
        pthread_mutex_unlock(&mutex);
        return false;
    }
    *a = buffer[head];
    *b = buffer_copy[head];
    if(head == tail)
        head = tail = -1;
    else
        head = (head + 1) % MAX_ENTRIES;
    pthread_mutex_unlock(&mutex);
    return true;
}

bool fifo_buffer::push(void *a, void *b) {
    pthread_mutex_lock(&mutex);
    if(tail == head - 1 || (head == 0 && tail == MAX_ENTRIES - 1)) {
        pthread_mutex_unlock(&mutex);
        return false;
    }
    if(tail == -1)
        tail = 0;
    else
        tail = (tail + 1) % MAX_ENTRIES;
    buffer[tail] = a;
    buffer_copy[tail] = b;
    if(head == -1)
        head = 0;
    pthread_mutex_unlock(&mutex);
    return true;
}

struct core {
    int id;
    uint64_t copies;
    fifo_buffer *send_buf;
    fifo_buffer *recv_buf;
    uint64_t page_faults;
} *cs;

#define WHILE_COND (TIME_DIFF(TIME_NOW, start) < max_time * 1000)

static void *sender_run(void *arg)
{
    struct core *cs = (struct core*)arg;
    int id = cs->id;
    struct fifo_buffer *send_buff = cs->send_buf;
    struct fifo_buffer *recv_buff = cs->recv_buf;

    auto start = TIME_NOW;
    uint64_t i = 0;
    do {
        void *buff = aligned_alloc(PAGE_SIZE, sizeof(char) * max_bytes);
        //char *buff = new char[max_bytes * (MAX_NUM_COPYS) + PAGE_SIZE];
        //buff = (void*)((uint64_t)buff + (PAGE_SIZE - ((uint64_t)buff & (PAGE_SIZE - 1))));
        void *buff_copy = aligned_alloc(PAGE_SIZE, sizeof(char) * max_bytes * (MAX_NUM_COPYS));
        //char *buff_copy = new char[max_bytes * (MAX_NUM_COPYS) + PAGE_SIZE];
        //buff_copy = (void*)((uint64_t)buff_copy + (PAGE_SIZE - ((uint64_t)buff_copy & (PAGE_SIZE - 1))));
        while(!send_buff->push(buff, buff_copy) && WHILE_COND);
        if(recv_buff->pop(&buff, &buff_copy)) {
            free(buff);
            free(buff_copy);
        }
    } while(WHILE_COND);
    return NULL;
}

static void* cleaner_run(void *arg)
{
    struct core *cs = (struct core*)arg;
    int id = cs->id;
    struct fifo_buffer *send_buf = cs->send_buf;
    struct fifo_buffer *recv_buf = cs->recv_buf;

    auto start = TIME_NOW;
    uint64_t i = 0;
    do {
        void *buff = NULL, *buff_copy = NULL;
        while(!recv_buf->pop(&buff, &buff_copy) && WHILE_COND);
        if(buff)
            free(buff);
        if(buff_copy)
            free(buff_copy);
    } while(WHILE_COND);
    return NULL;
}

static void *receiver_run(void *arg)
{
    struct core *cs = (struct core*)arg;
    int id = cs->id;
    struct fifo_buffer *send_buf = cs->send_buf;
    struct fifo_buffer *recv_buf = cs->recv_buf;
    //for(int i = 0; i < MAX_NUM_COPYS; ++i)
    //    recv(-2, &thread_buffer[i * max_bytes], max_bytes, 0);
    void *buff = aligned_alloc(PAGE_SIZE, max_bytes);
    void *buff_copy = aligned_alloc(PAGE_SIZE, max_bytes * MAX_NUM_COPYS);


    struct perf_event_attr pe_attr_page_faults;
    memset(&pe_attr_page_faults, 0, sizeof(pe_attr_page_faults));
    pe_attr_page_faults.size = sizeof(pe_attr_page_faults);
    pe_attr_page_faults.type =   PERF_TYPE_SOFTWARE;
    pe_attr_page_faults.config = PERF_COUNT_SW_PAGE_FAULTS;
    pe_attr_page_faults.disabled = 1;
    pe_attr_page_faults.exclude_kernel = 1;
    int page_faults_fd = perf_event_open(&pe_attr_page_faults, 0, -1, -1, 0);
    if (page_faults_fd == -1) {
        printf("perf_event_open failed for page faults: %s\n", strerror(errno));
        return NULL;
    }

    // Start counting
    ioctl(page_faults_fd, PERF_EVENT_IOC_RESET, 0);
    ioctl(page_faults_fd, PERF_EVENT_IOC_ENABLE, 0);

    int copies = 0;
    auto start = TIME_NOW;
    uint64_t i = 0;
    //int fd = open("foo.txt", O_RDONLY);
    while(WHILE_COND) {
    //for(int i = 0; i < 200; ++i) {
        //void *buff = aligned_alloc(PAGE_SIZE, max_bytes);
        //void *buff_copy = aligned_alloc(PAGE_SIZE, max_bytes * MAX_NUM_COPYS);
        //void *buff = NULL, *buff_copy = NULL;
        while(!send_buf->pop(&buff, &buff_copy) && WHILE_COND);
        if(buff && buff_copy) {
            recv(-2, buff, max_bytes, 0xdeadbeef);
            for(int i = 0; i < MAX_NUM_COPYS; ++i) {
                if(i == 0)
                    memcpy(&((char*)buff_copy)[0], 
                        &((char*)buff)[0], max_bytes);
                else
                    memcpy(&((char*)buff_copy)[i * max_bytes], 
                        &((char*)buff_copy)[(i - 1) * max_bytes], max_bytes);

                ++copies;
                //printf("Copy %d: %d\n", id, i);
            }
            while(!recv_buf->push(buff, buff_copy) && WHILE_COND);
        }
    }

    m5_exit(0);
    m5_memcpy_elide(buff_copy, buff, max_bytes);

    // Stop counting and read value
    ioctl(page_faults_fd, PERF_EVENT_IOC_DISABLE, 0);
    uint64_t page_faults_count;
    read(page_faults_fd, &page_faults_count, sizeof(page_faults_count));

    cs->copies = copies;
    cs->page_faults = page_faults_count;

    return NULL;
}

int main(int argc, char *argv[])
{
    unsigned num_threads;
    pthread_t *send_pts, *clean_pts, *recv_pts;

    if (argc != 4) {
        fprintf(stderr, "Usage: %s THREADS BYTES TIME\n", argv[0]);
        return EXIT_FAILURE;
    }

    num_threads = atoi(argv[1]);
    max_bytes = atoi(argv[2]);
    max_time = atoi(argv[3]);

    send_pts = (pthread_t*)calloc(num_threads, sizeof(pthread_t));
    clean_pts = (pthread_t*)calloc(num_threads, sizeof(pthread_t));
    recv_pts = (pthread_t*)calloc(num_threads, sizeof(pthread_t));
    cs = (core*)calloc(num_threads, sizeof(*cs));
    if (send_pts == NULL || clean_pts == NULL || recv_pts == NULL || cs == NULL) {
        fprintf(stderr, "allocating thread handles failed\n");
        return EXIT_FAILURE;
    }

    for (int i = 0; i < num_threads; i++) {
        cs[i].id = i;
        cs[i].copies = 0;
        cs[i].send_buf = (fifo_buffer*)malloc(sizeof(fifo_buffer));
        cs[i].send_buf->init();
        cs[i].recv_buf = (fifo_buffer*)malloc(sizeof(fifo_buffer));
        cs[i].recv_buf->init();
        if (pthread_create(send_pts + i, NULL, sender_run, &cs[i])) {
            fprintf(stderr, "pthread_create failed\n");
            return EXIT_FAILURE;
        }
        /*if (pthread_create(clean_pts + i, NULL, cleaner_run, &cs[i])) {
            fprintf(stderr, "pthread_create failed\n");
            return EXIT_FAILURE;
        }*/
        if (pthread_create(recv_pts + i, NULL, receiver_run, &cs[i])) {
            fprintf(stderr, "pthread_create failed\n");
            return EXIT_FAILURE;
        }
    }
    
    uint64_t copies = 0, page_faults_count = 0;
    auto start = TIME_NOW;
    for (int i = 0; i < num_threads; i++) {
        //pthread_join(send_pts[i], NULL);
        //pthread_join(clean_pts[i], NULL);
        pthread_join(recv_pts[i], NULL);
        copies += cs[i].copies;
        page_faults_count += cs[i].page_faults;
    }
    auto end = TIME_NOW;

    double total_time = TIME_DIFF(end, start) / 1000.0;
    printf("Copies = %ld\nSize = %ld bytes\nTime = %.2f s\nThroughput = %.2f mbps\n", copies, max_bytes,
        total_time, (double)(copies * max_bytes) / total_time / (1024.0 * 1024.0));
    printf("Page faults: %ld\n", page_faults_count);
    return EXIT_SUCCESS;
}



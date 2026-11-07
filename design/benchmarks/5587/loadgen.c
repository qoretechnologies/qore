// minimal keep-alive HTTP load generator: loadgen <port> <threads> <requests per thread>
// each thread has one connection and sends "GET / HTTP/1.1" repeatedly, reading the whole response each time
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define MAX_THREADS 256

static int port;
static long nreq;
static volatile long total_done = 0;
static volatile long errors = 0;

static void* run(void* arg) {
    (void)arg;
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in a = {0};
    a.sin_family = AF_INET;
    a.sin_port = htons(port);
    inet_pton(AF_INET, "127.0.0.1", &a.sin_addr);
    if (connect(fd, (struct sockaddr*)&a, sizeof a)) {
        __sync_fetch_and_add(&errors, 1);
        return NULL;
    }
    int one = 1;
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof one);
    const char req[] = "GET / HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n";
    char buf[4096];
    for (long i = 0; i < nreq; ++i) {
        if (write(fd, req, sizeof req - 1) != (ssize_t)(sizeof req - 1)) {
            __sync_fetch_and_add(&errors, 1);
            break;
        }
        // read until the end of the headers and the 2 byte body
        size_t have = 0;
        for (;;) {
            ssize_t n = read(fd, buf + have, sizeof buf - have - 1);
            if (n <= 0) {
                __sync_fetch_and_add(&errors, 1);
                close(fd);
                return NULL;
            }
            have += n;
            buf[have] = 0;
            char* e = strstr(buf, "\r\n\r\n");
            if (e && have >= (size_t)(e - buf) + 4 + 2) {
                break;
            }
        }
        if (strncmp(buf, "HTTP/1.1 200", 12)) {
            __sync_fetch_and_add(&errors, 1);
        }
        __sync_fetch_and_add(&total_done, 1);
    }
    close(fd);
    return NULL;
}

int main(int argc, char** argv) {
    if (argc != 4) {
        fprintf(stderr, "usage: loadgen <port> <threads 1-%d> <requests per thread>\n", MAX_THREADS);
        return 2;
    }
    port = atoi(argv[1]);
    int threads = atoi(argv[2]);
    nreq = atol(argv[3]);
    if (port < 1 || port > 65535 || threads < 1 || threads > MAX_THREADS || nreq < 1) {
        fprintf(stderr, "loadgen: invalid argument (port 1-65535, threads 1-%d, requests >= 1)\n", MAX_THREADS);
        return 2;
    }
    pthread_t t[MAX_THREADS];
    struct timespec s, e;
    clock_gettime(CLOCK_MONOTONIC, &s);
    int started = 0;
    for (int i = 0; i < threads; ++i) {
        if (pthread_create(&t[i], NULL, run, NULL)) {
            fprintf(stderr, "loadgen: cannot create thread %d\n", i);
            ++errors;
            break;
        }
        ++started;
    }
    for (int i = 0; i < started; ++i) {
        pthread_join(t[i], NULL);
    }
    clock_gettime(CLOCK_MONOTONIC, &e);
    double secs = (e.tv_sec - s.tv_sec) + (e.tv_nsec - s.tv_nsec) / 1e9;
    printf("requests=%ld errors=%ld seconds=%.2f req_per_sec=%.0f\n", total_done, errors, secs, total_done / secs);
    return 0;
}

/*
 * false_sharing.c - a controlled experiment for the report's cache discussion.
 *
 * This is NOT one of the four required versions.  It isolates the effect that
 * the OpenMP implementation avoids: several threads writing independent
 * counters that happen to live in the same 64-byte cache line.
 *
 * Variant "packed"   : volatile long long counters[MAX_THREADS];
 *                      counters[tid]++ in the loop.  Adjacent counters share
 *                      cache lines, so every write invalidates the line in the
 *                      other cores' caches (false sharing).
 * Variant "padded"   : one 64-byte-aligned block per thread, so each thread
 *                      writes a line no other thread touches.
 *
 * Compile: gcc -O2 -std=c11 -Wall -Wextra -Wpedantic -fopenmp \
 *              experiments/false_sharing.c -o bin/false_sharing
 * Run    : ./bin/false_sharing [iterations_per_thread]
 *
 * Output is CSV on stdout:
 *   variant,threads,run,seconds,increments,checksum
 */
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>

#define MAX_THREADS 64
#define CACHE_LINE 64

typedef struct {
    volatile long long value;
    unsigned char pad[CACHE_LINE - (sizeof(long long) % CACHE_LINE)];
} padded_counter_t;

_Static_assert(sizeof(padded_counter_t) % CACHE_LINE == 0,
               "padded counter must occupy whole cache lines");

static volatile long long g_packed[MAX_THREADS];
static padded_counter_t g_padded[MAX_THREADS] __attribute__((aligned(CACHE_LINE)));

static double run_packed(int threads, long long iters, long long *checksum)
{
    for (int i = 0; i < threads; i++)
        g_packed[i] = 0;

    double t0 = omp_get_wtime();
#pragma omp parallel num_threads(threads)
    {
        int tid = omp_get_thread_num();
        for (long long i = 0; i < iters; i++)
            g_packed[tid]++; /* separate variables, same cache line */
    }
    double t1 = omp_get_wtime();

    long long sum = 0;
    for (int i = 0; i < threads; i++)
        sum += g_packed[i];
    *checksum = sum;
    return t1 - t0;
}

static double run_padded(int threads, long long iters, long long *checksum)
{
    for (int i = 0; i < threads; i++)
        g_padded[i].value = 0;

    double t0 = omp_get_wtime();
#pragma omp parallel num_threads(threads)
    {
        int tid = omp_get_thread_num();
        for (long long i = 0; i < iters; i++)
            g_padded[tid].value++; /* private cache line */
    }
    double t1 = omp_get_wtime();

    long long sum = 0;
    for (int i = 0; i < threads; i++)
        sum += g_padded[i].value;
    *checksum = sum;
    return t1 - t0;
}

int main(int argc, char **argv)
{
    long long iters = 20000000LL;
    if (argc > 1)
        iters = atoll(argv[1]);
    if (iters <= 0) {
        fprintf(stderr, "usage: %s [iterations_per_thread > 0]\n", argv[0]);
        return 1;
    }

    printf("variant,threads,run,seconds,increments,checksum\n");

    int configs[] = {1, 2, 4, 8};
    for (int c = 0; c < 4; c++) {
        int threads = configs[c];
        if (threads > MAX_THREADS)
            continue;
        for (int run = 1; run <= 5; run++) {
            long long sum = 0;
            /* Alternate order so neither variant always runs first. */
            double tp, td;
            if (run % 2 == 1) {
                tp = run_packed(threads, iters, &sum);
                printf("packed,%d,%d,%.6f,%lld,%lld\n", threads, run, tp,
                       iters * threads, sum);
                td = run_padded(threads, iters, &sum);
                printf("padded,%d,%d,%.6f,%lld,%lld\n", threads, run, td,
                       iters * threads, sum);
            } else {
                td = run_padded(threads, iters, &sum);
                printf("padded,%d,%d,%.6f,%lld,%lld\n", threads, run, td,
                       iters * threads, sum);
                tp = run_packed(threads, iters, &sum);
                printf("packed,%d,%d,%.6f,%lld,%lld\n", threads, run, tp,
                       iters * threads, sum);
            }
        }
    }
    return 0;
}

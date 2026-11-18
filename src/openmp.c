/*
 * openmp.c - shared-memory parallel version (fork-join, OpenMP).
 *
 * Strategy
 * --------
 *  - One parallel region; the analysis loop is a static `omp for` so each
 *    thread owns a contiguous slice of the compact arrays (cache friendly).
 *  - Scalar sums/counts use OpenMP `reduction(+:...)`.  No critical section and
 *    no atomic operation is executed inside the loop.
 *  - Highest/lowest and Top 10 use one private padded block per thread.  The
 *    blocks are merged deterministically after the loop.
 *  - Each thread block is padded to a whole number of 64-byte cache lines so
 *    two threads never write to the same cache line (false sharing).
 */
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "output.h"
#include "stats.h"

#define MAX_THREADS 128
#define CACHE_LINE 64

typedef struct {
    stats_t agg;
    /* Pad the block to a 64-byte multiple.  sizeof(stats_t) is 208 bytes in
     * this build, so the pad is 48 bytes and every slot is 256 bytes. */
    unsigned char pad[CACHE_LINE - (sizeof(stats_t) % CACHE_LINE)];
} padded_stats_t;

_Static_assert(sizeof(padded_stats_t) % CACHE_LINE == 0,
               "per-thread stats block must be cache-line aligned");

static padded_stats_t g_locals[MAX_THREADS] __attribute__((aligned(CACHE_LINE)));

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage: %s <dataset.csv>\n", argv[0]);
        return 1;
    }

    dataset_t ds;
    char err[256];
    double t_start = omp_get_wtime();

    if (dataset_load(argv[1], &ds, err, sizeof(err)) != 0) {
        fprintf(stderr, "error: %s\n", err);
        return 1;
    }
    double t_read = omp_get_wtime();

    int nthreads = omp_get_max_threads();
    if (nthreads > MAX_THREADS) {
        fprintf(stderr, "error: %d threads requested, limit is %d\n", nthreads,
                MAX_THREADS);
        dataset_free(&ds);
        return 1;
    }
    if (nthreads < 1)
        nthreads = 1;

    long long g_students = 0;
    long long g_total = 0;
    long long g_s1 = 0, g_s2 = 0, g_s3 = 0, g_s4 = 0, g_s5 = 0;
    long long g_p1 = 0, g_p2 = 0, g_p3 = 0, g_p4 = 0, g_p5 = 0;
    long long g_passed = 0, g_failed = 0;

#pragma omp parallel num_threads(nthreads) reduction(+ : g_students, g_total,    \
                                                     g_s1, g_s2, g_s3, g_s4,   \
                                                     g_s5, g_p1, g_p2, g_p3,   \
                                                     g_p4, g_p5, g_passed,     \
                                                     g_failed)
    {
        int tid = omp_get_thread_num();
        stats_t *local = &g_locals[tid].agg;
        stats_init(local);

#pragma omp for schedule(static)
        for (long long i = 0; i < ds.count; i++) {
            const unsigned char *m = ds.marks + i * 5;
            int v1 = m[0], v2 = m[1], v3 = m[2], v4 = m[3], v5 = m[4];
            int total = v1 + v2 + v3 + v4 + v5;

            g_students++;
            g_total += total;
            g_s1 += v1;
            g_s2 += v2;
            g_s3 += v3;
            g_s4 += v4;
            g_s5 += v5;
            if (v1 >= PASS_MARK)
                g_p1++;
            if (v2 >= PASS_MARK)
                g_p2++;
            if (v3 >= PASS_MARK)
                g_p3++;
            if (v4 >= PASS_MARK)
                g_p4++;
            if (v5 >= PASS_MARK)
                g_p5++;
            if (v1 >= PASS_MARK && v2 >= PASS_MARK && v3 >= PASS_MARK &&
                v4 >= PASS_MARK && v5 >= PASS_MARK)
                g_passed++;
            else
                g_failed++;

            stats_add_aggregates(local, ds.rolls[i], m);
        }
    }

    double t_analysis_done = omp_get_wtime();

    /* Merge per-thread aggregates, then fill in the reduction results. */
    stats_t st;
    stats_init(&st);
    for (int t = 0; t < nthreads; t++) {
        stats_merge_aggregates(&st, &g_locals[t].agg);
    }
    st.students = g_students;
    st.total_sum = g_total;
    st.subject_sum[0] = g_s1;
    st.subject_sum[1] = g_s2;
    st.subject_sum[2] = g_s3;
    st.subject_sum[3] = g_s4;
    st.subject_sum[4] = g_s5;
    st.subject_pass[0] = g_p1;
    st.subject_pass[1] = g_p2;
    st.subject_pass[2] = g_p3;
    st.subject_pass[3] = g_p4;
    st.subject_pass[4] = g_p5;
    st.passed = g_passed;
    st.failed = g_failed;

    results_t res;
    stats_finalize(&st, &res);
    double analysis = t_analysis_done - t_read;
    output_print(&res, analysis);
    output_print_timing(t_read - t_start, analysis, t_analysis_done - t_start);

    dataset_free(&ds);
    return 0;
}

/*
 * hybrid.c - MPI + OpenMP version.
 *
 *  - MPI distributes row blocks exactly like mpi.c (rank 0 reads and scatters).
 *  - Inside each MPI process, an OpenMP team analyzes the local block:
 *    sums/counts by reduction, extremes/Top 10 in padded per-thread blocks.
 *  - MPI is initialized with MPI_Init_thread(..., MPI_THREAD_FUNNELED, ...):
 *    only the master thread (the one that called MPI_Init_thread) ever calls
 *    MPI.  The parallel region is joined before any collective is entered.
 *  - Only rank 0 prints, and it reports the slowest rank's analysis time.
 */
#include <mpi.h>
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "output.h"
#include "stats.h"

#define SUM_SLOTS 14
#define MAX_THREADS 128
#define CACHE_LINE 64

typedef struct {
    stats_t agg;
    unsigned char pad[CACHE_LINE - (sizeof(stats_t) % CACHE_LINE)];
} padded_stats_t;

_Static_assert(sizeof(padded_stats_t) % CACHE_LINE == 0,
               "per-thread stats block must be cache-line aligned");

static padded_stats_t g_locals[MAX_THREADS] __attribute__((aligned(CACHE_LINE)));

int main(int argc, char **argv)
{
    int rank, size, provided = MPI_THREAD_SINGLE;

    MPI_Init_thread(&argc, &argv, MPI_THREAD_FUNNELED, &provided);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (rank == 0 && provided < MPI_THREAD_FUNNELED) {
        fprintf(stderr,
                "warning: MPI provided thread level %d < MPI_THREAD_FUNNELED; "
                "this code only calls MPI from the master thread anyway\n",
                provided);
    }

    if (argc != 2) {
        if (rank == 0)
            fprintf(stderr, "usage: %s <dataset.csv>\n", argv[0]);
        MPI_Finalize();
        return 1;
    }

    double t_start = MPI_Wtime();

    dataset_t ds;
    dataset_init(&ds);
    long long n = 0;

    if (rank == 0) {
        char err[256];
        if (dataset_load(argv[1], &ds, err, sizeof(err)) != 0) {
            fprintf(stderr, "error: %s\n", err);
            n = -1;
        } else {
            n = ds.count;
        }
    }
    MPI_Bcast(&n, 1, MPI_LONG_LONG, 0, MPI_COMM_WORLD);
    if (n < 0) {
        MPI_Finalize();
        return 1;
    }

    long long base = n / size;
    int rem = (int)(n % size);

    int *counts = (int *)malloc((size_t)size * sizeof(int));
    int *displs = (int *)malloc((size_t)size * sizeof(int));
    int *mcounts = (int *)malloc((size_t)size * sizeof(int));
    int *mdispls = (int *)malloc((size_t)size * sizeof(int));

    for (int r = 0; r < size; r++) {
        counts[r] = (int)(base + (r < rem ? 1 : 0));
        displs[r] = (int)((long long)r * base + (r < rem ? r : rem));
        mcounts[r] = counts[r] * 5;
        mdispls[r] = displs[r] * 5;
    }

    int my_rows = counts[rank];
    int *my_rolls = (int *)malloc(sizeof(int) * (size_t)(my_rows > 0 ? my_rows : 1));
    unsigned char *my_marks =
        (unsigned char *)malloc((size_t)(my_rows > 0 ? my_rows : 1) * 5);

    MPI_Scatterv((rank == 0) ? ds.rolls : NULL, counts, displs, MPI_INT, my_rolls,
                 my_rows, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Scatterv((rank == 0) ? ds.marks : NULL, mcounts, mdispls,
                 MPI_UNSIGNED_CHAR, my_marks, my_rows * 5,
                 MPI_UNSIGNED_CHAR, 0, MPI_COMM_WORLD);

    MPI_Barrier(MPI_COMM_WORLD);
    double t_read = MPI_Wtime();
    double read_time = (rank == 0) ? (t_read - t_start) : 0.0;

    int nthreads = omp_get_max_threads();
    if (nthreads > MAX_THREADS)
        nthreads = MAX_THREADS;
    if (nthreads < 1)
        nthreads = 1;

    double a_start = MPI_Wtime();

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
        for (int i = 0; i < my_rows; i++) {
            const unsigned char *m = my_marks + (size_t)i * 5;
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

            stats_add_aggregates(local, my_rolls[i], m);
        }
    }

    stats_t st;
    stats_init(&st);
    for (int t = 0; t < nthreads; t++)
        stats_merge_aggregates(&st, &g_locals[t].agg);
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

    /* --- MPI collectives: executed by the master thread only --- */
    long long sbuf[SUM_SLOTS];
    long long rbuf[SUM_SLOTS];
    sbuf[0] = st.students;
    sbuf[1] = st.total_sum;
    for (int s = 0; s < NUM_SUBJECTS; s++) {
        sbuf[2 + s] = st.subject_sum[s];
        sbuf[7 + s] = st.subject_pass[s];
    }
    sbuf[12] = st.passed;
    sbuf[13] = st.failed;
    MPI_Reduce(sbuf, rbuf, SUM_SLOTS, MPI_LONG_LONG, MPI_SUM, 0, MPI_COMM_WORLD);

    int hp[2] = {st.high.mark, st.high.loc};
    int lp[2] = {st.low.mark, st.low.loc};
    int gh[2] = {0, 0};
    int gl[2] = {0, 0};
    MPI_Reduce(hp, gh, 1, MPI_2INT, MPI_MAXLOC, 0, MPI_COMM_WORLD);
    MPI_Reduce(lp, gl, 1, MPI_2INT, MPI_MINLOC, 0, MPI_COMM_WORLD);

    int local_top[TOP_N * 2];
    for (int i = 0; i < TOP_N; i++) {
        local_top[i * 2] = st.top[i].roll;
        local_top[i * 2 + 1] = st.top[i].total;
    }

    int *all_top = NULL;
    if (rank == 0)
        all_top = (int *)malloc((size_t)size * TOP_N * 2 * sizeof(int));
    MPI_Gather(local_top, TOP_N * 2, MPI_INT, all_top, TOP_N * 2, MPI_INT, 0,
               MPI_COMM_WORLD);

    double a_end = MPI_Wtime();
    double local_analysis = a_end - a_start;
    double analysis = 0.0;
    MPI_Reduce(&local_analysis, &analysis, 1, MPI_DOUBLE, MPI_MAX, 0,
               MPI_COMM_WORLD);

    if (rank == 0) {
        stats_t global;
        stats_init(&global);
        global.students = rbuf[0];
        global.total_sum = rbuf[1];
        for (int s = 0; s < NUM_SUBJECTS; s++) {
            global.subject_sum[s] = rbuf[2 + s];
            global.subject_pass[s] = rbuf[7 + s];
        }
        global.passed = rbuf[12];
        global.failed = rbuf[13];
        global.high.mark = gh[0];
        global.high.loc = gh[1];
        global.low.mark = gl[0];
        global.low.loc = gl[1];

        for (int r = 0; r < size; r++) {
            const int *block = all_top + (size_t)r * TOP_N * 2;
            for (int j = 0; j < TOP_N; j++) {
                int roll = block[j * 2];
                int total = block[j * 2 + 1];
                if (roll >= 0)
                    stats_offer_top(&global, roll, total);
            }
        }

        results_t res;
        stats_finalize(&global, &res);
        output_print(&res, analysis);
        output_print_timing(read_time, analysis, read_time + analysis);
        free(all_top);
    }

    dataset_free(&ds);
    free(counts);
    free(displs);
    free(mcounts);
    free(mdispls);
    free(my_rolls);
    free(my_marks);

    MPI_Finalize();
    return 0;
}

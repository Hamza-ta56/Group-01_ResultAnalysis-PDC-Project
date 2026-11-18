/*
 * mpi.c - distributed-memory version (MPI, rank 0 reads and scatters).
 *
 * Data flow
 * ---------
 *   rank 0: read CSV -> rolls[] + marks[] (5 bytes/student)
 *   all   : compute per-rank row counts/displacements (uneven division)
 *   scatterv rolls (MPI_INT) and marks (MPI_UNSIGNED_CHAR)
 *   all   : analyze only their own slice (point of distributed memory)
 *   reduce: 14 long long sums/counts with MPI_SUM
 *           highest/lowest with MPI_MAXLOC / MPI_MINLOC on MPI_2INT
 *   gather: each rank's local Top 10 -> rank 0 merges and prints
 *
 * Only rank 0 prints.  Analysis timing is the slowest rank's time, obtained
 * with MPI_Reduce(MPI_MAX).
 */
#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "output.h"
#include "stats.h"

#define SUM_SLOTS 14 /* students, total, 5 subject sums, 5 subject passes,
                        passed, failed */

int main(int argc, char **argv)
{
    int rank, size;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

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

    /* Uneven division: base/rem, rank r gets base + (r < rem). */
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

    /* The barrier both completes the distribution and aligns all ranks before
     * the analysis timer starts.  Read time includes load, parse, scatter. */
    MPI_Barrier(MPI_COMM_WORLD);
    double t_read = MPI_Wtime();
    double read_time = (rank == 0) ? (t_read - t_start) : 0.0;

    double a_start = MPI_Wtime();

    stats_t st;
    stats_init(&st);
    for (int i = 0; i < my_rows; i++)
        stats_add_row(&st, my_rolls[i], my_marks + (size_t)i * 5);

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

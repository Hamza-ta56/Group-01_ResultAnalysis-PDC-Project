/*
 * sequential.c - baseline implementation.
 *
 * Reads the CSV into memory, analyzes it in one pass, prints the eight
 * statistics.  It is the correctness reference for every parallel version.
 *
 * Build: gcc -std=c11 -O2 -Wall -Wextra -Wpedantic -fopenmp ...
 */
#include <omp.h>
#include <stdio.h>

#include "common.h"
#include "output.h"
#include "stats.h"

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

    stats_t st;
    stats_init(&st);
    const unsigned char *marks = ds.marks;
    for (long long i = 0; i < ds.count; i++) {
        stats_add_row(&st, ds.rolls[i], marks + i * 5);
    }
    double t_done = omp_get_wtime();

    results_t res;
    stats_finalize(&st, &res);
    double analysis = t_done - t_read;
    output_print(&res, analysis);
    output_print_timing(t_read - t_start, analysis, t_done - t_start);

    dataset_free(&ds);
    return 0;
}

/*
 * stats.h - shared statistics engine.
 *
 * Every version (sequential, OpenMP, MPI, hybrid) calls the very same
 * functions, which is what guarantees bit-identical output.
 *
 * A location code is   loc = roll_no * 10 + subject_number   (subject 1..5).
 * It makes highest/lowest tie-breaking total, cheap and deterministic:
 *   highest: larger mark wins, smaller loc wins ties
 *   lowest : smaller mark wins, smaller loc wins ties
 */
#ifndef STATS_H
#define STATS_H

#include <limits.h>

#define NUM_SUBJECTS 5
#define PASS_MARK 50
#define TOP_N 10

typedef struct {
    int mark; /* -1 = "no mark seen" sentinel for maximum                 */
    int loc;  /* roll * 10 + subject; INT_MAX = "no location" sentinel    */
} extremum_t;

typedef struct {
    int roll;  /* -1 marks an unused slot                                  */
    int total; /* integer total out of 500, never a floating average       */
} top_entry_t;

typedef struct {
    long long students;
    long long total_sum;                 /* all marks, all subjects           */
    long long subject_sum[NUM_SUBJECTS];
    long long subject_pass[NUM_SUBJECTS];
    long long passed;                    /* passed all five subjects          */
    long long failed;                    /* failed at least one subject       */
    extremum_t high;
    extremum_t low;
    top_entry_t top[TOP_N];
} stats_t;

/* Full row accumulation: sums, counts, extremes, Top 10. */
void stats_init(stats_t *s);
void stats_add_row(stats_t *s, int roll, const unsigned char marks[NUM_SUBJECTS]);

/* Aggregates only (extremes + Top 10).  Used by the OpenMP loops where the
 * scalar sums/counts are accumulated by OpenMP reduction variables. */
void stats_add_aggregates(stats_t *s, int roll,
                          const unsigned char marks[NUM_SUBJECTS]);

/* Merge a whole local stats block into dst (sums + aggregates). */
void stats_merge(stats_t *dst, const stats_t *src);

/* Merge only extremes + Top 10 (used after OpenMP reductions). */
void stats_merge_aggregates(stats_t *dst, const stats_t *src);

/* Offer one (roll, total) candidate to a Top 10 list. */
void stats_offer_top(stats_t *s, int roll, int total);

/* Convert accumulated sums to printable floating-point results. */
typedef struct {
    long long students;
    double overall_average;
    int high_mark, high_roll, high_subject;
    int low_mark, low_roll, low_subject;
    long long passed, failed;
    double subject_average[NUM_SUBJECTS];
    double subject_pass_rate[NUM_SUBJECTS];
    int top_count;
    int top_roll[TOP_N];
    double top_average[TOP_N];
} results_t;

void stats_finalize(const stats_t *s, results_t *r);

#endif /* STATS_H */

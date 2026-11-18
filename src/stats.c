/*
 * stats.c — shared, deterministic statistics engine.
 *
 * All arithmetic on marks is integer (long long); division into floating point
 * happens only once, in stats_finalize().  This is required because ten
 * million students times five subjects times 100 marks is 5,000,000,000,
 * which does not fit in a 32-bit int.
 */
#include "stats.h"

#include <stddef.h>
#include <string.h>

static int top_is_better(int total_a, int roll_a, int total_b, int roll_b)
{
    if (total_a != total_b)
        return total_a > total_b;
    return roll_a < roll_b; /* tie: smaller roll number first */
}

void stats_init(stats_t *s)
{
    memset(s, 0, sizeof(*s));
    s->high.mark = -1;
    s->high.loc = INT_MAX;
    s->low.mark = 101;
    s->low.loc = INT_MAX;
    for (int i = 0; i < TOP_N; i++) {
        s->top[i].roll = -1;
        s->top[i].total = -1;
    }
}

static void update_high(stats_t *s, int mark, int loc)
{
    if (mark > s->high.mark || (mark == s->high.mark && loc < s->high.loc)) {
        s->high.mark = mark;
        s->high.loc = loc;
    }
}

static void update_low(stats_t *s, int mark, int loc)
{
    if (mark < s->low.mark || (mark == s->low.mark && loc < s->low.loc)) {
        s->low.mark = mark;
        s->low.loc = loc;
    }
}

void stats_offer_top(stats_t *s, int roll, int total)
{
    int idx = -1;

    /* Find the first position this candidate beats; invalid slots are worst. */
    for (int i = 0; i < TOP_N; i++) {
        if (s->top[i].roll < 0 ||
            top_is_better(total, roll, s->top[i].total, s->top[i].roll)) {
            idx = i;
            break;
        }
    }
    if (idx < 0)
        return; /* does not make the Top 10 */

    for (int i = TOP_N - 1; i > idx; i--)
        s->top[i] = s->top[i - 1];

    s->top[idx].roll = roll;
    s->top[idx].total = total;
}

void stats_add_aggregates(stats_t *s, int roll,
                          const unsigned char marks[NUM_SUBJECTS])
{
    int total = 0;

    for (int sub = 0; sub < NUM_SUBJECTS; sub++) {
        int m = (int)marks[sub];
        int loc = roll * 10 + (sub + 1);
        update_high(s, m, loc);
        update_low(s, m, loc);
        total += m;
    }
    stats_offer_top(s, roll, total);
}

void stats_add_row(stats_t *s, int roll,
                   const unsigned char marks[NUM_SUBJECTS])
{
    int total = 0;
    int all_pass = 1;

    for (int sub = 0; sub < NUM_SUBJECTS; sub++) {
        int m = (int)marks[sub];
        total += m;
        s->subject_sum[sub] += m;
        if (m >= PASS_MARK)
            s->subject_pass[sub]++;
        else
            all_pass = 0;
    }

    s->students++;
    s->total_sum += total;
    if (all_pass)
        s->passed++;
    else
        s->failed++;

    stats_add_aggregates(s, roll, marks);
}

static void merge_extremes(stats_t *dst, const stats_t *src)
{
    update_high(dst, src->high.mark, src->high.loc);
    update_low(dst, src->low.mark, src->low.loc);
}

void stats_merge_aggregates(stats_t *dst, const stats_t *src)
{
    merge_extremes(dst, src);
    for (int i = 0; i < TOP_N; i++) {
        if (src->top[i].roll >= 0)
            stats_offer_top(dst, src->top[i].roll, src->top[i].total);
    }
}

void stats_merge(stats_t *dst, const stats_t *src)
{
    dst->students += src->students;
    dst->total_sum += src->total_sum;
    for (int sub = 0; sub < NUM_SUBJECTS; sub++) {
        dst->subject_sum[sub] += src->subject_sum[sub];
        dst->subject_pass[sub] += src->subject_pass[sub];
    }
    dst->passed += src->passed;
    dst->failed += src->failed;
    stats_merge_aggregates(dst, src);
}

void stats_finalize(const stats_t *s, results_t *r)
{
    memset(r, 0, sizeof(*r));
    r->students = s->students;

    if (s->students > 0) {
        r->overall_average =
            (double)s->total_sum / ((double)s->students * NUM_SUBJECTS);
        for (int sub = 0; sub < NUM_SUBJECTS; sub++) {
            r->subject_average[sub] =
                (double)s->subject_sum[sub] / (double)s->students;
            r->subject_pass_rate[sub] =
                100.0 * (double)s->subject_pass[sub] / (double)s->students;
        }
    }

    r->high_mark = s->high.mark;
    r->low_mark = s->low.mark;
    if (s->high.mark >= 0 && s->high.loc != INT_MAX) {
        r->high_roll = s->high.loc / 10;
        r->high_subject = s->high.loc % 10;
    }
    if (s->low.mark <= 100 && s->low.loc != INT_MAX) {
        r->low_roll = s->low.loc / 10;
        r->low_subject = s->low.loc % 10;
    }

    r->passed = s->passed;
    r->failed = s->failed;

    for (int i = 0; i < TOP_N; i++) {
        if (s->top[i].roll < 0)
            break;
        r->top_roll[r->top_count] = s->top[i].roll;
        r->top_average[r->top_count] = (double)s->top[i].total / 5.0;
        r->top_count++;
    }
}

#include "output.h"

#include <stdio.h>

void output_print(const results_t *r, double analysis_seconds)
{
    printf("Students: %lld\n", r->students);
    printf("Overall average: %.4f\n", r->overall_average);
    printf("Highest mark: %d(roll %d, S%d)\n", r->high_mark, r->high_roll,
           r->high_subject);
    printf("Lowest mark: %d(roll %d, S%d)\n", r->low_mark, r->low_roll,
           r->low_subject);
    printf("Passed/ Failed: %lld/ %lld\n", r->passed, r->failed);

    printf("Subject averages:");
    for (int s = 0; s < NUM_SUBJECTS; s++)
        printf(" S%d %.4f", s + 1, r->subject_average[s]);
    printf("\n");

    printf("Subject pass rate:");
    for (int s = 0; s < NUM_SUBJECTS; s++)
        printf(" S%d %.2f%%", s + 1, r->subject_pass_rate[s]);
    printf("\n");

    printf("Top 10:");
    for (int i = 0; i < r->top_count; i++)
        printf("%s roll %d(%.2f)", (i == 0) ? "" : ",", r->top_roll[i],
               r->top_average[i]);
    printf("\n");

    printf("Time(analysis): %.4f s\n", analysis_seconds);
}

void output_print_timing(double read_seconds, double analysis_seconds,
                         double total_seconds)
{
    fprintf(stderr, "TIMING read=%.6f analysis=%.6f total=%.6f\n", read_seconds,
            analysis_seconds, total_seconds);
}

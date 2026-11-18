/*
 * output.h — the single canonical output formatter.
 *
 * All four versions print through this module, so the correctness check can
 * compare whole stdout streams with diff.
 */
#ifndef OUTPUT_H
#define OUTPUT_H

#include "stats.h"

/* Prints the eight required statistics followed by the timing line. */
void output_print(const results_t *r, double analysis_seconds);

/* Machine-readable timing line on stderr:
 *   TIMING read=<s> analysis=<s> total=<s>
 */
void output_print_timing(double read_seconds, double analysis_seconds,
                         double total_seconds);

#endif /* OUTPUT_H */

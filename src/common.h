/*
 * common.h - shared CSV dataset loader used by all four versions.
 *
 * Storage layout (chosen for cache efficiency and cheap MPI scatter):
 *   rolls[i]          : int            (4 bytes per student)
 *   marks[i*5 + s]    : unsigned char  (5 bytes per student)
 */
#ifndef COMMON_H
#define COMMON_H

#include <stddef.h>

typedef struct {
    long long count;        /* number of data rows successfully parsed        */
    int *rolls;             /* count entries                                  */
    unsigned char *marks;   /* count * 5 entries, student-major (S1..S5)      */
} dataset_t;

void dataset_init(dataset_t *ds);
void dataset_free(dataset_t *ds);

/*
 * Load a CSV file of the form:
 *   roll_no,S1,S2,S3,S4,S5
 *   1,67,45,89,72,55
 *   ...
 *
 * Returns 0 on success, -1 on failure (message written into errbuf).
 * Malformed data lines are skipped with a warning on stderr; the number of
 * skipped lines is reported at the end.  The header is skipped automatically.
 */
int dataset_load(const char *path, dataset_t *ds, char *errbuf, size_t errlen);

#endif /* COMMON_H */

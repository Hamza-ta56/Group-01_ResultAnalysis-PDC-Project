/*
 * common.c — shared streaming CSV reader.
 *
 * Design notes
 * ------------
 *  - The whole file is read into one buffer with fread() (one syscall per big
 *    chunk), then parsed with a hand-written integer scanner.  This is much
 *    faster than fscanf()/strtok() and keeps the read phase measurable.
 *  - Rows are stored in two compact arrays: int rolls and 5 unsigned chars of
 *    marks per student.  A 10,000,000-student file therefore needs ~90 MB
 *    after parsing, instead of several hundred MB for a struct-of-ints layout.
 *  - The parser accepts both LF and CRLF line endings (the generator writes
 *    CRLF when compiled with MinGW on Windows).
 */
#include "common.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void dataset_init(dataset_t *ds)
{
    ds->count = 0;
    ds->rolls = NULL;
    ds->marks = NULL;
}

void dataset_free(dataset_t *ds)
{
    free(ds->rolls);
    free(ds->marks);
    ds->rolls = NULL;
    ds->marks = NULL;
    ds->count = 0;
}

static int ensure_capacity(dataset_t *ds, long long *capacity, long long need)
{
    if (need <= *capacity)
        return 0;

    long long newcap = (*capacity > 0) ? *capacity : 1024;
    while (newcap < need) {
        long long step = newcap / 2 + 1024;
        newcap += step;
    }

    int *new_rolls = (int *)realloc(ds->rolls, (size_t)newcap * sizeof(int));
    if (new_rolls == NULL)
        return -1;
    ds->rolls = new_rolls;

    unsigned char *new_marks =
        (unsigned char *)realloc(ds->marks, (size_t)newcap * 5u);
    if (new_marks == NULL)
        return -1;
    ds->marks = new_marks;

    *capacity = newcap;
    return 0;
}

/* Parse one non-negative decimal integer starting at *pp, stopping at 'end'.
 * Returns 1 on success, 0 if no digit was found. */
static int scan_uint(const char **pp, const char *end, int *out)
{
    const char *p = *pp;
    int value = 0;
    int digits = 0;

    while (p < end && (*p == ' ' || *p == '\t'))
        p++;

    while (p < end && *p >= '0' && *p <= '9') {
        value = value * 10 + (*p - '0');
        p++;
        digits++;
        if (value > 100000000) /* defensive: refuse absurd values */
            return 0;
    }

    if (!digits)
        return 0;

    *pp = p;
    *out = value;
    return 1;
}

int dataset_load(const char *path, dataset_t *ds, char *errbuf, size_t errlen)
{
    FILE *f = NULL;
    char *buf = NULL;
    long long capacity = 0;
    long long line_no = 0;
    long long bad_lines = 0;
    int rc = -1;

    dataset_init(ds);

    f = fopen(path, "rb");
    if (f == NULL) {
        snprintf(errbuf, errlen, "cannot open '%s': %s", path, strerror(errno));
        return -1;
    }

    if (fseek(f, 0, SEEK_END) != 0) {
        snprintf(errbuf, errlen, "cannot seek '%s'", path);
        goto done;
    }
    long file_size = ftell(f);
    if (file_size < 0) {
        snprintf(errbuf, errlen, "cannot determine size of '%s'", path);
        goto done;
    }
    rewind(f);

    buf = (char *)malloc((size_t)file_size + 1);
    if (buf == NULL) {
        snprintf(errbuf, errlen, "out of memory reading '%s' (%ld bytes)", path,
                 file_size);
        goto done;
    }

    size_t got = fread(buf, 1, (size_t)file_size, f);
    if (got != (size_t)file_size) {
        snprintf(errbuf, errlen, "short read on '%s' (got %zu of %ld bytes)",
                 path, got, file_size);
        goto done;
    }
    buf[got] = '\0';

    /* Reserve a generous initial capacity from the file size.  The average
     * generated row is ~20 bytes, so file_size/16 rarely triggers a grow. */
    if (ensure_capacity(ds, &capacity, (long long)got / 16 + 1024) != 0) {
        snprintf(errbuf, errlen, "out of memory allocating row arrays");
        goto done;
    }

    const char *p = buf;
    const char *end = buf + got;

    /* Skip the header line if present. */
    if ((size_t)(end - p) >= 7 && strncmp(p, "roll_no", 7) == 0) {
        while (p < end && *p != '\n')
            p++;
        if (p < end)
            p++;
        line_no = 1;
    }

    while (p < end) {
        const char *line_start = p;
        const char *nl = (const char *)memchr(p, '\n', (size_t)(end - p));
        const char *line_end = (nl != NULL) ? nl : end;

        /* Trim trailing CR / spaces. */
        const char *q = line_end;
        while (q > line_start &&
               (q[-1] == '\r' || q[-1] == ' ' || q[-1] == '\t'))
            q--;

        if (q == line_start) { /* empty line */
            p = (nl != NULL) ? nl + 1 : end;
            line_no++;
            continue;
        }

        int roll = 0;
        int m[5] = {0, 0, 0, 0, 0};
        int ok = 1;
        const char *cur = line_start;

        if (!scan_uint(&cur, q, &roll))
            ok = 0;
        for (int s = 0; ok && s < 5; s++) {
            while (cur < q && (*cur == ' ' || *cur == '\t'))
                cur++;
            if (cur >= q || *cur != ',') {
                ok = 0;
                break;
            }
            cur++; /* comma */
            if (!scan_uint(&cur, q, &m[s]) || m[s] < 0 || m[s] > 100)
                ok = 0;
        }
        if (ok) {
            while (cur < q && (*cur == ' ' || *cur == '\t'))
                cur++;
            if (cur != q) /* trailing garbage */
                ok = 0;
        }
        if (ok && roll <= 0)
            ok = 0;

        if (!ok) {
            bad_lines++;
            if (bad_lines <= 10) {
                fprintf(stderr,
                        "warning: %s:%lld: malformed row skipped\n", path,
                        line_no + 1);
            }
        } else {
            if (ensure_capacity(ds, &capacity, ds->count + 1) != 0) {
                snprintf(errbuf, errlen, "out of memory growing row arrays");
                goto done;
            }
            long long i = ds->count;
            ds->rolls[i] = roll;
            for (int s = 0; s < 5; s++)
                ds->marks[i * 5 + s] = (unsigned char)m[s];
            ds->count++;
        }

        p = (nl != NULL) ? nl + 1 : end;
        line_no++;
    }

    if (bad_lines > 10)
        fprintf(stderr, "warning: %s: %lld malformed rows skipped in total\n",
                path, bad_lines);

    if (ds->count == 0) {
        snprintf(errbuf, errlen, "'%s' contained no valid data rows", path);
        goto done;
    }

    rc = 0;

done:
    if (f != NULL)
        fclose(f);
    free(buf);
    if (rc != 0)
        dataset_free(ds);
    return rc;
}

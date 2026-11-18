# Design Decisions

This file records the design choices for the first checkpoint: shared core, sequential version, OpenMP version, and MPI version.

## Shared core

The reader, statistics engine, and printer are shared by every version. This keeps the output identical and avoids copied formulas.

## Data layout

Each student is stored as one `int` roll number and five `unsigned char` marks. This uses 9 bytes per student instead of a larger structure of integers. A 10 million row dataset uses about 90 MB after parsing.

## Statistics

All sums and counts use `long long`. Division happens once in `stats_finalize`, after all accumulation is complete. Extrema and the top ten use integer totals, not floating point values, so tie rules are deterministic.

The tie key is:

```
location = roll * 10 + subject
```

For the highest mark, a larger mark wins. If two marks are equal, the smaller location wins. The same rule applies to the lowest mark.

## Sequential version

The sequential version reads the file into one buffer, parses it once, and performs one pass over the rows. It is the correctness reference for every parallel version.

## OpenMP version

Rows are divided with `#pragma omp parallel for schedule(static)`. Sums and counts use a reduction clause. Each thread keeps its own highest, lowest, and top ten candidates. Thread blocks are aligned and padded to reduce false sharing. The per-thread results are merged after the parallel region.

## MPI version

Rank 0 reads the CSV file and broadcasts the number of students. Rows are divided as evenly as possible:

```
base = n / p
rem  = n % p
rows(r) = base + (1 if r < rem else 0)
row_offset(r) = r * base + min(r, rem)
```

The rolls are scattered as `MPI_INT`. The marks are scattered as `MPI_UNSIGNED_CHAR`. Mark counts and offsets are multiplied by five because each row has five marks.

Local sums and counts are combined with `MPI_Reduce` and `MPI_SUM`. Highest and lowest are combined with `MPI_MAXLOC` and `MPI_MINLOC` on `MPI_2INT` pairs. Each rank prepares ten candidates, and rank 0 gathers and merges them.

Only rank 0 prints the result. The analysis timer starts after `MPI_Barrier` so every rank begins at the same point. The reported analysis time is the slowest rank time, produced with `MPI_Reduce` and `MPI_MAX`.

## Timing

The read time covers opening, reading, and parsing the file. The analysis time covers statistics and, for MPI, the collectives. The total time is the sum of read and analysis. Timing is printed to standard error in one line:

```
TIMING read=... analysis=... total=...
```

## Planned next steps

The hybrid MPI and OpenMP version, correctness automation, benchmarks, graphs, and report are added in later milestones.

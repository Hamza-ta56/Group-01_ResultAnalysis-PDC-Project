# Group-01_ResultAnalysis

CS-401 Parallel and Distributed Computing semester project.

International Islamic University Islamabad.

## Team

| Member | Roll number |
|---|---|
| Sajidullah Khan | 4995-FOC/BSCS/F23-B |
| Hamza Tahir | 4962-FOC/BSCS/F23-B |

## Project

The program reads a CSV file of student roll numbers and marks in five subjects. It reports eight statistics: total students, overall average, highest mark, lowest mark, passed and failed counts, subject averages, subject pass rates, and the top ten students.

## Current status

Checkpoint for 24 November 2026.

Completed:

- shared CSV reader
- shared statistics engine
- shared output format
- sequential version
- OpenMP version
- MPI version
- automated correctness harness (scripts/check_correctness.sh)
- dataset generation targets (small, medium, large) and Colab setup script
- benchmark driver (scripts/run_benchmarks.sh)
- machine information collector (scripts/machine_info.sh)
- result summarization script (scripts/summarize_results.py)
- plotting script (scripts/make_plots.py)

Tested configurations on the small dataset:

- Sequential baseline
- OpenMP with 1, 2, 4, 8 threads
- MPI with 1, 2, 4, 8 processes

Planned for later commits:

- hybrid MPI and OpenMP version
- false-sharing microbenchmark study
- full benchmark runs and summary tables across all datasets
- final report and packaging targets

## Repository layout

```
Group-01_ResultAnalysis/
├── README.md
├── Makefile
├── src/
│   ├── common.h, common.c      shared CSV reader
│   ├── stats.h, stats.c        shared statistics engine
│   ├── output.h, output.c      shared output
│   ├── sequential.c            sequential version
│   ├── openmp.c                OpenMP version
│   ├── mpi.c                   MPI version
│   └── gen_students.c          provided dataset generator
├── scripts/
│   ├── lib.sh                  shared shell helpers
│   ├── check_correctness.sh    exact-diff correctness harness
│   ├── setup_colab.sh          Colab and Ubuntu setup script
│   ├── run_benchmarks.sh       benchmark driver
│   ├── machine_info.sh         machine specification collector
│   ├── summarize_results.py    result summarization script
│   └── make_plots.py           plot generation script
├── tools/
│   ├── mpicc                   Windows MPI compiler wrapper
│   └── mpirun                  Windows MPI launcher wrapper
├── results/
│   ├── correctness/            small dataset test logs (.out and .err)
│   └── machine_info.txt        collected machine specification
├── data/                       generated datasets, not committed
└── bin/                        compiled programs, not committed
```

## Requirements

- GCC with C11 support
- GNU Make
- OpenMP support in GCC
- MPI compiler and launcher (Open MPI on Linux or Microsoft MPI on Windows)
- Python 3 (standard library for summarizer, matplotlib for plotting)

## Build

```bash
make all
```

This builds:

```
bin/seq
bin/omp
bin/mpi
```

On Windows the files end in `.exe`.

## Create datasets

Build the provided generator:

```bash
make gen
```

Generate datasets:

```bash
make datasets
```

Or generate individual datasets:

```bash
./bin/gen_students 100000 data/small.csv
./bin/gen_students 1000000 data/medium.csv
./bin/gen_students 10000000 data/large.csv
```

## Run

Sequential version:

```bash
./bin/seq data/small.csv
```

OpenMP version with four threads:

```bash
OMP_NUM_THREADS=4 ./bin/omp data/small.csv
```

MPI version with four processes:

```bash
mpiexec -n 4 ./bin/mpi data/small.csv
```

On Windows, use the provided launcher:

```bash
bash tools/mpirun -n 4 ./bin/mpi data/small.csv
```

## Test and correctness

Run the basic test targets:

```bash
make test
```

Run the exact-diff correctness check on small dataset:

```bash
DATASETS="small" make correctness
```

This compares each parallel configuration against the sequential baseline with `diff` after removing the `Time(analysis):` line.

## Benchmarks and analysis

Collect machine information:

```bash
make machine-info
```

Run benchmark repetitions:

```bash
DATASETS="small" RUNS=5 make bench
```

Summarize benchmark results into medians, speedup, and efficiency:

```bash
make summarize
```

Generate graphs from summarized results:

```bash
make plots
```

## Output

The program prints the eight statistics to standard output. It also prints a timing line to standard error:

```
TIMING read=... analysis=... total=...
```

## Notes

- Generated dataset files (data/*.csv) are not committed.
- Compiled programs (bin/*) are not committed.
- The hybrid version, false-sharing experiment, full benchmark results, report, and packaging are added in later milestones.

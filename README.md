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

Checkpoint for 18 November 2026.

Completed:

- shared CSV reader
- shared statistics engine
- shared output format
- sequential version
- OpenMP version
- MPI version

Planned for later commits:

- hybrid MPI and OpenMP version
- automated correctness and benchmark scripts
- graphs and final report

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
├── tools/
│   ├── mpicc                   Windows MPI compiler wrapper
│   └── mpirun                  Windows MPI launcher wrapper
├── data/                       generated datasets, not committed
└── bin/                        compiled programs, not committed
```

## Requirements

- GCC with C11 support
- GNU Make
- OpenMP support in GCC
- MPI compiler and launcher (Open MPI on Linux or Microsoft MPI on Windows)

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

## Create a dataset

Build the provided generator first:

```bash
make gen
mkdir -p data
./bin/gen_students 100000 data/small.csv
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

## Output

The program prints the statistics to standard output. It also prints a timing line to standard error:

```
TIMING read=... analysis=... total=...
```

## Notes

- Generated dataset files are not committed.
- Compiled programs are not committed.
- The hybrid version, benchmark scripts, graphs, and report are added in later milestones.

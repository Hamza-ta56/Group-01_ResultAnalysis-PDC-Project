# CS-401 Parallel and Distributed Computing semester project
# Group-01_ResultAnalysis
#
# Targets: all gen seq omp mpi datasets test clean

CC       = gcc
MPICC   ?= $(shell command -v mpicc 2>/dev/null || echo tools/mpicc)
CFLAGS   = -std=c11 -O2 -Wall -Wextra -Wpedantic
OMPFLAGS = -fopenmp
LDFLAGS  =
GROUP   ?= Group-01

ifeq ($(OS),Windows_NT)
EXE := .exe
else
EXE :=
endif

BIN     := bin
DATA    := data
CORE    := src/common.c src/stats.c src/output.c
HEADERS := src/common.h src/stats.h src/output.h

SEQ     := $(BIN)/seq$(EXE)
OMP     := $(BIN)/omp$(EXE)
MPIBIN  := $(BIN)/mpi$(EXE)
GEN     := $(BIN)/gen_students$(EXE)

.PHONY: all gen seq omp mpi datasets test correctness bench machine-info clean

all: seq omp mpi

# Provided dataset generator. Do not modify its behaviour.
gen: $(GEN)

$(GEN): src/gen_students.c | $(BIN)
	$(CC) -std=c11 -O2 -Wall -Wextra -Wpedantic $< -o $@

seq: $(SEQ)
omp: $(OMP)
mpi: $(MPIBIN)

$(SEQ): src/sequential.c $(CORE) $(HEADERS) | $(BIN)
	$(CC) $(CFLAGS) $(OMPFLAGS) $(LDFLAGS) $(CORE) src/sequential.c -o $@

$(OMP): src/openmp.c $(CORE) $(HEADERS) | $(BIN)
	$(CC) $(CFLAGS) $(OMPFLAGS) $(LDFLAGS) $(CORE) src/openmp.c -o $@

$(MPIBIN): src/mpi.c $(CORE) $(HEADERS) | $(BIN)
	$(MPICC) $(CFLAGS) $(LDFLAGS) $(CORE) src/mpi.c -o $@

$(BIN):
	@mkdir -p $(BIN)

datasets:
	@mkdir -p $(DATA)
	./$(GEN) 100000 $(DATA)/small.csv
	./$(GEN) 1000000 $(DATA)/medium.csv
	./$(GEN) 10000000 $(DATA)/large.csv

test: all
	@echo "== sequential on small =="
	./$(SEQ) $(DATA)/small.csv
	@echo "== OpenMP 4 threads on small =="
	OMP_NUM_THREADS=4 ./$(OMP) $(DATA)/small.csv

correctness: all
	bash scripts/check_correctness.sh

bench: all
	bash scripts/run_benchmarks.sh

machine-info:
	bash scripts/machine_info.sh

clean:
	rm -rf $(BIN)
	@echo "cleaned binaries"

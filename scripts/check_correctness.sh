#!/usr/bin/env bash
# scripts/check_correctness.sh
#
# Builds nothing by itself: run `make all` first (the Makefile target does it).
# Compares every parallel configuration against the sequential baseline with
# diff, after deleting only the "Time(analysis):" line.
#
# Usage:  bash scripts/check_correctness.sh
# Env:    DATASETS="small medium"   (default; set to include large)
#         SKIP_BUILD=1
set -uo pipefail

source "$(dirname "$0")/lib.sh"
require_binaries

DATASETS="${DATASETS:-small medium}"
LOG="$ROOT/results/correctness"
mkdir -p "$LOG"

pass=0
fail=0
declare -a failures=()

check_one() {
    local label="$1" dataset="$2" file="$3"
    local base="$LOG/${dataset}_seq.out"
    if diff -q "$base" "$file" >/dev/null 2>&1; then
        echo "[PASS] $dataset $label"
        pass=$((pass + 1))
    else
        echo "[FAIL] $dataset $label"
        diff -u "$base" "$file" > "$LOG/${dataset}_${label//[^A-Za-z0-9_.-]/_}.diff"
        fail=$((fail + 1))
        failures+=("$dataset $label")
    fi
}

echo "=============================================================="
echo " Correctness check - baseline is bin/seq"
echo " Datasets: $DATASETS"
echo "=============================================================="

for ds in $DATASETS; do
    csv="$ROOT/data/$ds.csv"
    if [ ! -f "$csv" ]; then
        echo "error: missing $csv (run make datasets-$ds)" >&2
        exit 1
    fi

    echo "--- $ds: sequential baseline ---"
    "$BINSEQ" "$csv" 2>"$LOG/${ds}_seq.err" | strip_time > "$LOG/${ds}_seq.out"
    cat "$LOG/${ds}_seq.out" | sed 's/^/    /'

    echo "--- $ds: OpenMP ---"
    for t in 1 2 4 8; do
        OMP_NUM_THREADS=$t "$BINOMP" "$csv" 2>"$LOG/${ds}_omp_t${t}.err" \
            | strip_time > "$LOG/${ds}_omp_t${t}.out"
        check_one "OpenMP ${t} thread(s)" "$ds" "$LOG/${ds}_omp_t${t}.out"
    done

    echo "--- $ds: MPI ---"
    for p in 1 2 4 8; do
        run_mpi "$p" "$BINMPI" "$csv" 2>"$LOG/${ds}_mpi_p${p}.err" \
            | strip_time > "$LOG/${ds}_mpi_p${p}.out"
        check_one "MPI ${p} process(es)" "$ds" "$LOG/${ds}_mpi_p${p}.out"
    done

    echo "--- $ds: hybrid ---"
    if [ ! -f "$BINHYB" ]; then
        echo "skip hybrid: bin/hybrid not built"
    else
        for combo in "1 8" "2 4" "4 2" "8 1"; do
            p="${combo% *}"
            t="${combo#* }"
            ( export OMP_NUM_THREADS="$t"
              run_mpi "$p" "$BINHYB" "$csv" ) \
                2>"$LOG/${ds}_hybrid_${p}x${t}.err" \
                | strip_time > "$LOG/${ds}_hybrid_${p}x${t}.out"
            check_one "hybrid ${p}x${t}" "$ds" "$LOG/${ds}_hybrid_${p}x${t}.out"
        done
    fi
done

echo "=============================================================="
echo " RESULT: $pass passed, $fail failed"
echo " Logs:   $LOG"
if [ "$fail" -ne 0 ]; then
    printf ' Failed: %s\n' "${failures[@]}"
    exit 1
fi
echo " All configurations match the sequential baseline exactly."

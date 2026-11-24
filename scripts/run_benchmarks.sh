#!/usr/bin/env bash
# scripts/run_benchmarks.sh
#
# Runs every required configuration RUNS times (default 5) on the selected
# datasets and appends one row per run to results/raw_timings.csv.
#
# Raw CSV columns:
#   dataset,version,p_processes,p_threads,p_total,run,read_time,analysis_time,total_time,output_file
#
# Usage:  bash scripts/run_benchmarks.sh
# Env:    DATASETS="small medium large"  RUNS=5  SKIP_BUILD=1
set -uo pipefail

source "$(dirname "$0")/lib.sh"
require_binaries

DATASETS="${DATASETS:-small medium large}"
RUNS="${RUNS:-5}"
OUTDIR="$ROOT/results/run_outputs"
RAW="$ROOT/results/raw_timings.csv"
mkdir -p "$OUTDIR" "$ROOT/results"

if [ ! -f "$RAW" ]; then
    echo "dataset,version,p_processes,p_threads,p_total,run,read_time,analysis_time,total_time,output_file" > "$RAW"
fi

record() {
    local dataset="$1" version="$2" pp="$3" pt="$4" run="$5" errfile="$6" outfile="$7"
    local read analysis total ptotal
    read="$(timing_field "$errfile" read)"
    analysis="$(timing_field "$errfile" analysis)"
    total="$(timing_field "$errfile" total)"
    if [ -z "$read" ] || [ -z "$analysis" ]; then
        echo "warning: no TIMING line for $dataset $version p=${pp} t=${pt} run $run" >&2
        return
    fi
    ptotal=$((pp * pt))
    echo "$dataset,$version,$pp,$pt,$ptotal,$run,$read,$analysis,$total,$outfile" >> "$RAW"
}

run_case() {
    local dataset="$1" version="$2" pp="$3" pt="$4" csv="$5"
    local i outfile errfile
    for i in $(seq 1 "$RUNS"); do
        outfile="$OUTDIR/${dataset}_${version}_p${pp}_t${pt}_run${i}.txt"
        errfile="$OUTDIR/${dataset}_${version}_p${pp}_t${pt}_run${i}.err"
        case "$version" in
            seq)
                "$BINSEQ" "$csv" >"$outfile" 2>"$errfile"
                ;;
            omp)
                OMP_NUM_THREADS="$pt" "$BINOMP" "$csv" >"$outfile" 2>"$errfile"
                ;;
            mpi)
                run_mpi "$pp" "$BINMPI" "$csv" >"$outfile" 2>"$errfile"
                ;;
            hybrid)
                ( export OMP_NUM_THREADS="$pt"
                  run_mpi "$pp" "$BINHYB" "$csv" ) >"$outfile" 2>"$errfile"
                ;;
        esac
        record "$dataset" "$version" "$pp" "$pt" "$i" "$errfile" "$outfile"
        printf '  %-7s %-6s p=%-2s t=%-2s run %s/%s  read=%ss analysis=%ss\n' \
            "$dataset" "$version" "$pp" "$pt" "$i" "$RUNS" \
            "$(timing_field "$errfile" read)" "$(timing_field "$errfile" analysis)"
    done
}

echo "=============================================================="
echo " Benchmarks: $RUNS runs per configuration"
echo " Datasets:   $DATASETS"
echo " Raw CSV:    $RAW"
echo "=============================================================="

for ds in $DATASETS; do
    csv="$ROOT/data/$ds.csv"
    if [ ! -f "$csv" ]; then
        echo "warning: $csv missing, skipping $ds" >&2
        continue
    fi
    echo "--- $ds ---"
    run_case "$ds" seq 1 1 "$csv"
    for t in 1 2 4 8; do run_case "$ds" omp 1 "$t" "$csv"; done
    for p in 1 2 4 8; do run_case "$ds" mpi "$p" 1 "$csv"; done
    if [ ! -f "$BINHYB" ]; then
        echo "skip hybrid: bin/hybrid not built"
    else
        for combo in "1 8" "2 4" "4 2" "8 1"; do
            p="${combo% *}"; th="${combo#* }"
            run_case "$ds" hybrid "$p" "$th" "$csv"
        done
    fi
done

echo "done: $RAW"

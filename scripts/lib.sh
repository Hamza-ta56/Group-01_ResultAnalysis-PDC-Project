#!/usr/bin/env bash
# scripts/lib.sh - helpers shared by the correctness and benchmark scripts.
# Sourced, never executed directly.

set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT" || exit 1

case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*) EXE=".exe" ;;
    *)                    EXE=""    ;;
esac

# Open MPI's mpirun, or the MS-MPI compatibility wrapper on Windows.
MPIRUN_BIN="${MPIRUN:-}"
if [ -z "$MPIRUN_BIN" ]; then
    if command -v mpirun >/dev/null 2>&1; then
        MPIRUN_BIN="$(command -v mpirun)"
    elif command -v mpiexec >/dev/null 2>&1; then
        MPIRUN_BIN="$(command -v mpiexec)"
    else
        MPIRUN_BIN="$ROOT/tools/mpirun"
    fi
fi

MPI_EXTRA=()
if "$MPIRUN_BIN" --version 2>&1 | grep -qi "open mpi"; then
    MPI_EXTRA+=(--oversubscribe)          # harmless when cores are enough
    if [ "$(id -u 2>/dev/null || echo 1)" = "0" ]; then
        MPI_EXTRA+=(--allow-run-as-root)  # Colab / containers
    fi
fi

BIN="$ROOT/bin"
BINSEQ="$BIN/seq$EXE"
BINOMP="$BIN/omp$EXE"
BINMPI="$BIN/mpi$EXE"
BINHYB="$BIN/hybrid$EXE"

run_mpi() {
    local np="$1"
    shift
    "$MPIRUN_BIN" "${MPI_EXTRA[@]}" -n "$np" "$@"
}

strip_time() {
    sed '/^Time(analysis):/d'
}

# Extract one field from the machine-readable stderr line:
#   TIMING read=... analysis=... total=...
timing_field() {
    local file="$1" key="$2"
    sed -n "s/.*${key}=\([0-9.][0-9.]*\).*/\1/p" "$file" | tail -n 1
}

require_binaries() {
    for b in "$BINSEQ" "$BINOMP" "$BINMPI"; do
        if [ ! -f "$b" ]; then
            echo "error: missing $b - run 'make all' first" >&2
            exit 1
        fi
    done
    if [ ! -f "$BINHYB" ]; then
        echo "note: $BINHYB is not built yet; hybrid runs will be skipped" >&2
    fi
}

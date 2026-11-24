#!/usr/bin/env bash
# scripts/machine_info.sh - collect the machine specification required by the
# project document and write it to results/machine_info.txt.
set -uo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="$ROOT/results/machine_info.txt"
mkdir -p "$ROOT/results"

{
    echo "===== Machine information ====="
    echo "Collected: $(date -u '+%Y-%m-%d %H:%M:%S UTC')"
    echo
    echo "--- Operating system ---"
    uname -a 2>/dev/null || true
    if [ -f /etc/os-release ]; then
        cat /etc/os-release
    fi
    if command -v powershell.exe >/dev/null 2>&1; then
        powershell.exe -NoProfile -Command \
            "(Get-CimInstance Win32_OperatingSystem | Select-Object Caption,Version,OSArchitecture | Format-List | Out-String).Trim()" 2>/dev/null || true
    fi
    echo
    echo "--- CPU ---"
    if command -v lscpu >/dev/null 2>&1; then
        lscpu
    elif command -v powershell.exe >/dev/null 2>&1; then
        powershell.exe -NoProfile -Command \
            "Get-CimInstance Win32_Processor | Select-Object Name,NumberOfCores,NumberOfLogicalProcessors | Format-List" 2>/dev/null || true
    fi
    echo "nproc: $(nproc 2>/dev/null || echo unknown)"
    echo
    echo "--- Memory ---"
    if command -v free >/dev/null 2>&1; then
        free -h
    elif command -v powershell.exe >/dev/null 2>&1; then
        powershell.exe -NoProfile -Command \
            "Get-CimInstance Win32_ComputerSystem | Select-Object TotalPhysicalMemory | Format-List" 2>/dev/null || true
    fi
    echo
    echo "--- Compilers and MPI ---"
    (gcc --version | head -n 1) 2>&1 || true
    # `mpicc` has three possible shapes: Open MPI driver (supports --version),
    # the MS-MPI compatibility wrapper in /ucrt64/bin (rejects --version), or
    # our tools/mpicc shim.  Report whatever can be determined without failing.
    MPICC_BIN="$(command -v mpicc 2>/dev/null || true)"
    echo "mpicc path: ${MPICC_BIN:-<none; Makefile falls back to tools/mpicc>}"
    if [ -n "$MPICC_BIN" ]; then
        echo -n "mpicc backend: "
        mpicc --version 2>&1 | head -n 1 | grep -v '^Unknown option' || true
    fi
    if command -v mpirun >/dev/null 2>&1; then
        mpirun --version 2>&1 | head -n 3
    elif [ -x "$ROOT/tools/mpirun" ]; then
        echo "launcher: Microsoft MPI (mpiexec), via tools/mpirun shim"
        if command -v powershell.exe >/dev/null 2>&1; then
            powershell.exe -NoProfile -Command \
                "\$p='C:\Program Files\Microsoft MPI\Bin\mpiexec.exe'; if (Test-Path \$p) { 'MS-MPI version: ' + (Get-Item \$p).VersionInfo.ProductVersion }" 2>/dev/null || true
        fi
    fi
    echo
    echo "--- OpenMP ---"
    echo "OMP_NUM_THREADS=${OMP_NUM_THREADS:-<unset: runtime default>}"
    echo
    echo "--- Python (plotting only) ---"
    # Prefer a real interpreter.  The WindowsApps `python3` entry is a Store
    # stub (and the `py` launcher may auto-install a runtime), so both are
    # skipped deliberately: probing versions must never modify the machine.
    PY=""
    for cand in python3 python; do
        p="$(command -v "$cand" 2>/dev/null || true)"
        case "$p" in
            ""|*WindowsApps*) ;;
            *) PY="$p"; break ;;
        esac
    done
    if [ -n "$PY" ]; then
        "$PY" --version 2>&1
    else
        echo "Python: not found (plotting scripts only)"
    fi
} > "$OUT" 2>&1

echo "written: $OUT"

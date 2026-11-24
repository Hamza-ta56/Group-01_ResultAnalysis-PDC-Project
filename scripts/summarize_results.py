#!/usr/bin/env python3
"""scripts/summarize_results.py

Read results/raw_timings.csv (one row per run), compute the median read /
analysis / total time per configuration, then the speedup and efficiency
against the sequential baseline of the same dataset.

Outputs
-------
results/summary.csv
results/tables/<dataset>_table.md
results/tables/all_tables.md

Only the Python standard library is used, so this runs anywhere.
"""
from __future__ import annotations

import csv
import os
import statistics
import sys
from collections import defaultdict

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
RAW = os.path.join(ROOT, "results", "raw_timings.csv")
SUMMARY = os.path.join(ROOT, "results", "summary.csv")
TABLES = os.path.join(ROOT, "results", "tables")

DATASETS = ["small", "medium", "large"]
VERSION_ORDER = {"seq": 0, "omp": 1, "mpi": 2, "hybrid": 3}


def load_rows(path):
    rows = []
    with open(path, newline="", encoding="utf-8") as fh:
        for row in csv.DictReader(fh):
            if not row.get("analysis_time"):
                continue
            rows.append(
                {
                    "dataset": row["dataset"],
                    "version": row["version"],
                    "pp": int(row["p_processes"]),
                    "pt": int(row["p_threads"]),
                    "p": int(row["p_total"]),
                    "run": int(row["run"]),
                    "read": float(row["read_time"]),
                    "analysis": float(row["analysis_time"]),
                    "total": float(row["total_time"]),
                }
            )
    return rows


def config_sort_key(version, pp, pt):
    if version == "hybrid":
        return (3, pp, pt)
    return (VERSION_ORDER.get(version, 9), pp, pt)


def p_label(version, pp, pt, p):
    if version == "hybrid":
        return f"{pp}×{pt} ({p})"
    return str(p)


def main():
    if not os.path.exists(RAW):
        sys.exit(f"error: {RAW} not found - run scripts/run_benchmarks.sh first")

    rows = load_rows(RAW)
    groups = defaultdict(list)
    for r in rows:
        groups[(r["dataset"], r["version"], r["pp"], r["pt"], r["p"])].append(r)

    summary = []
    for key, runs in groups.items():
        dataset, version, pp, pt, p = key
        summary.append(
            {
                "dataset": dataset,
                "version": version,
                "pp": pp,
                "pt": pt,
                "p": p,
                "runs": len(runs),
                "read": statistics.median(r["read"] for r in runs),
                "analysis": statistics.median(r["analysis"] for r in runs),
                "total": statistics.median(r["total"] for r in runs),
            }
        )

    baseline = {
        s["dataset"]: s["analysis"] for s in summary if s["version"] == "seq"
    }

    os.makedirs(TABLES, exist_ok=True)
    with open(SUMMARY, "w", newline="", encoding="utf-8") as fh:
        writer = csv.writer(fh)
        writer.writerow(
            [
                "dataset",
                "version",
                "p_processes",
                "p_threads",
                "p_total",
                "runs",
                "median_read_s",
                "median_analysis_s",
                "median_total_s",
                "speedup",
                "efficiency_pct",
            ]
        )
        for s in sorted(
            summary,
            key=lambda x: (
                DATASETS.index(x["dataset"]) if x["dataset"] in DATASETS else 99,
                config_sort_key(x["version"], x["pp"], x["pt"]),
            ),
        ):
            t1 = baseline.get(s["dataset"])
            speedup = t1 / s["analysis"] if t1 and s["analysis"] > 0 else 0.0
            eff = 100.0 * speedup / s["p"] if s["p"] else 0.0
            s["speedup"] = speedup
            s["efficiency"] = eff
            writer.writerow(
                [
                    s["dataset"],
                    s["version"],
                    s["pp"],
                    s["pt"],
                    s["p"],
                    s["runs"],
                    f"{s['read']:.6f}",
                    f"{s['analysis']:.6f}",
                    f"{s['total']:.6f}",
                    f"{speedup:.3f}",
                    f"{eff:.1f}",
                ]
            )

    all_tables = []
    for ds in DATASETS:
        entries = [
            s
            for s in summary
            if s["dataset"] == ds and "speedup" in s
        ]
        if not entries:
            continue
        entries.sort(key=lambda x: config_sort_key(x["version"], x["pp"], x["pt"]))
        label = {"seq": "Sequential", "omp": "OpenMP", "mpi": "MPI", "hybrid": "Hybrid"}
        lines = [
            f"### Dataset `{ds}.csv`",
            "",
            "| Version | p | Read time(s) | Analysis time(s) | Speedup | Efficiency(%) |",
            "|---|---|---|---|---|---|",
        ]
        for s in entries:
            lines.append(
                "| {v} | {p} | {r:.4f} | {a:.4f} | {s:.2f} | {e:.1f} |".format(
                    v=label[s["version"]],
                    p=p_label(s["version"], s["pp"], s["pt"], s["p"]),
                    r=s["read"],
                    a=s["analysis"],
                    s=s["speedup"],
                    e=s["efficiency"],
                )
            )
        text = "\n".join(lines) + "\n"
        with open(os.path.join(TABLES, f"{ds}_table.md"), "w", encoding="utf-8") as fh:
            fh.write(text)
        all_tables.append(text)

    with open(os.path.join(TABLES, "all_tables.md"), "w", encoding="utf-8") as fh:
        fh.write("# Results tables\n\n")
        fh.write("Speedup and efficiency are computed from the median analysis "
                 "time, relative to the sequential baseline on the same dataset.\n\n")
        fh.write("\n".join(all_tables))

    print(f"wrote {SUMMARY}")
    print(f"wrote {TABLES}/*_table.md")


if __name__ == "__main__":
    main()

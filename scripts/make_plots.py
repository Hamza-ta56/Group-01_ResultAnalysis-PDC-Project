#!/usr/bin/env python3
"""scripts/make_plots.py - generate the four required PNG graphs.

Input : results/summary.csv  (produced by summarize_results.py)
Output: results/graphs/speedup_large.png
        results/graphs/efficiency_large.png
        results/graphs/analysis_time_vs_dataset.png
        results/graphs/read_vs_analysis_stacked.png
"""
from __future__ import annotations

import csv
import os
import sys

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SUMMARY = os.path.join(ROOT, "results", "summary.csv")
GRAPHS = os.path.join(ROOT, "results", "graphs")

DATASETS = ["small", "medium", "large"]
SIZES = {"small": 100_000, "medium": 1_000_000, "large": 10_000_000}
VERSIONS = ["omp", "mpi", "hybrid"]
LABELS = {"seq": "Sequential", "omp": "OpenMP", "mpi": "MPI", "hybrid": "Hybrid"}
MARKERS = {"omp": "o", "mpi": "s", "hybrid": "^"}
COLORS = {"omp": "#1f77b4", "mpi": "#d62728", "hybrid": "#2ca02c"}


def load():
    rows = []
    with open(SUMMARY, newline="", encoding="utf-8") as fh:
        for row in csv.DictReader(fh):
            row["pp"] = int(row["p_processes"])
            row["pt"] = int(row["p_threads"])
            row["p"] = int(row["p_total"])
            row["read"] = float(row["median_read_s"])
            row["analysis"] = float(row["median_analysis_s"])
            row["speedup"] = float(row["speedup"])
            row["eff"] = float(row["efficiency_pct"])
            rows.append(row)
    return rows


def by_version(rows, dataset, version):
    sel = [r for r in rows if r["dataset"] == dataset and r["version"] == version]
    return sorted(sel, key=lambda r: r["p"])


def best_config(rows, dataset, version):
    sel = [r for r in rows if r["dataset"] == dataset and r["version"] == version]
    return min(sel, key=lambda r: r["analysis"]) if sel else None


def plot_speedup(rows):
    fig, ax = plt.subplots(figsize=(7.2, 4.6))
    maxp = 1
    for v in VERSIONS:
        sel = by_version(rows, "large", v)
        if not sel:
            continue
        p = [r["p"] for r in sel]
        s = [r["speedup"] for r in sel]
        maxp = max(maxp, max(p))
        ax.plot(p, s, marker=MARKERS[v], color=COLORS[v], label=LABELS[v], lw=1.8)
    ideal_p = list(range(1, maxp + 1))
    ax.plot(ideal_p, ideal_p, "--", color="gray", lw=1.2, label="Ideal S = p")
    ax.set_xlabel("Workers p (threads / processes / processes×threads)")
    ax.set_ylabel("Speedup S(p) = T1 / Tp")
    ax.set_title("Speedup vs workers - large.csv")
    ax.set_xticks(sorted(set(ideal_p)))
    ax.grid(True, alpha=0.3)
    ax.legend()
    fig.tight_layout()
    fig.savefig(os.path.join(GRAPHS, "speedup_large.png"), dpi=160)
    plt.close(fig)


def plot_efficiency(rows):
    fig, ax = plt.subplots(figsize=(7.2, 4.6))
    for v in VERSIONS:
        sel = by_version(rows, "large", v)
        if not sel:
            continue
        ax.plot(
            [r["p"] for r in sel],
            [r["eff"] for r in sel],
            marker=MARKERS[v],
            color=COLORS[v],
            label=LABELS[v],
            lw=1.8,
        )
    ax.set_xlabel("Workers p")
    ax.set_ylabel("Efficiency E(p) = S(p)/p × 100%")
    ax.set_title("Efficiency vs workers - large.csv")
    ax.set_ylim(0, 110)
    ax.grid(True, alpha=0.3)
    ax.legend()
    fig.tight_layout()
    fig.savefig(os.path.join(GRAPHS, "efficiency_large.png"), dpi=160)
    plt.close(fig)


def plot_analysis_vs_size(rows):
    fig, ax = plt.subplots(figsize=(7.2, 4.6))
    for v in ["seq"] + VERSIONS:
        xs, ys = [], []
        for ds in DATASETS:
            best = best_config(rows, ds, v)
            if best:
                xs.append(SIZES[ds])
                ys.append(best["analysis"])
        if xs:
            ax.plot(xs, ys, marker="o", label=LABELS[v], lw=1.8)
    ax.set_xscale("log")
    ax.set_yscale("log")
    ax.set_xlabel("Dataset size (students, log scale)")
    ax.set_ylabel("Median analysis time (s, log scale)")
    ax.set_title("Analysis time vs dataset size (best configuration per version)")
    ax.grid(True, which="both", alpha=0.3)
    ax.legend()
    fig.tight_layout()
    fig.savefig(os.path.join(GRAPHS, "analysis_time_vs_dataset.png"), dpi=160)
    plt.close(fig)


def plot_read_vs_analysis(rows):
    fig, ax = plt.subplots(figsize=(7.2, 5.0))
    versions = ["seq"] + VERSIONS
    labels, reads, analyses = [], [], []
    for v in versions:
        best = best_config(rows, "large", v)
        if not best:
            continue
        labels.append(LABELS[v] + (f"\n({best['pp']}×{best['pt']})" if v == "hybrid" else f"\n(p={best['p']})"))
        reads.append(best["read"])
        analyses.append(best["analysis"])

    x = range(len(labels))
    ax.bar(x, reads, label="Read + parse (+ scatter)", color="#9ecae1")
    ax.bar(x, analyses, bottom=reads, label="Analysis", color="#3182bd")
    for i, (r, a) in enumerate(zip(reads, analyses)):
        ax.text(i, r + a + 0.02, f"T={r + a:.2f}s\nread {100 * r / (r + a):.0f}%",
                ha="center", va="bottom", fontsize=8)
    ax.set_xticks(list(x))
    ax.set_xticklabels(labels, fontsize=8)
    ax.set_ylabel("Time (s)")
    ax.set_title("Read vs analysis time - large.csv, best configuration")
    ax.set_ylim(0, max(r + a for r, a in zip(reads, analyses)) * 1.28)
    ax.legend(loc="upper center", bbox_to_anchor=(0.5, -0.20), ncol=2, frameon=False)
    ax.grid(True, axis="y", alpha=0.3)
    fig.tight_layout()
    fig.savefig(os.path.join(GRAPHS, "read_vs_analysis_stacked.png"), dpi=160)
    plt.close(fig)


def main():
    if not os.path.exists(SUMMARY):
        sys.exit(f"error: {SUMMARY} not found - run scripts/summarize_results.py")
    os.makedirs(GRAPHS, exist_ok=True)
    rows = load()
    plot_speedup(rows)
    plot_efficiency(rows)
    plot_analysis_vs_size(rows)
    plot_read_vs_analysis(rows)
    print(f"wrote 4 PNG files to {GRAPHS}")


if __name__ == "__main__":
    main()

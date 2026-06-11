#!/usr/bin/env python3
"""Gera graficos agregados a partir de results/benchmark_raw.csv."""

from __future__ import annotations

import argparse
from pathlib import Path

import matplotlib.pyplot as plt
import pandas as pd

METRICS = [
    ("avg_insert_probes", "Media de sondas por insercao"),
    ("avg_search_success_probes", "Media de sondas por busca bem-sucedida"),
    ("max_cluster", "Maior cluster (media experimental)"),
    ("worst_insert_probes", "Pior caso de sondas na insercao"),
    ("insert_ms", "Tempo medio de insercao (ms)"),
]


def aggregate(df: pd.DataFrame) -> pd.DataFrame:
    grouped = (
        df.groupby(["strategy", "load_factor"])
        .agg(
            avg_insert_probes=("avg_insert_probes", "mean"),
            avg_search_success_probes=("avg_search_success_probes", "mean"),
            max_cluster=("max_cluster", "mean"),
            worst_insert_probes=("worst_insert_probes", "mean"),
            insert_ms=("insert_ms", "mean"),
        )
        .reset_index()
    )
    return grouped


def plot_metric(agg: pd.DataFrame, column: str, title: str, out_dir: Path) -> None:
    plt.figure(figsize=(8, 5))
    for strategy, part in agg.groupby("strategy"):
        part = part.sort_values("load_factor")
        plt.plot(part["load_factor"], part[column], marker="o", label=strategy)

    plt.xlabel("Fator de carga (alpha)")
    plt.ylabel(title)
    plt.title(title)
    plt.grid(True, alpha=0.3)
    plt.legend()
    plt.tight_layout()
    out_dir.mkdir(parents=True, exist_ok=True)
    plt.savefig(out_dir / f"{column}.png", dpi=160)
    plt.close()


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", default="results/benchmark_raw.csv")
    parser.add_argument("--output", default="results/plots")
    args = parser.parse_args()

    df = pd.read_csv(args.input)
    agg = aggregate(df)
    agg.to_csv(Path(args.output).parent / "benchmark_summary.csv", index=False)

    out_dir = Path(args.output)
    for column, title in METRICS:
        plot_metric(agg, column, title, out_dir)

    print(f"Graficos salvos em {out_dir}")


if __name__ == "__main__":
    main()

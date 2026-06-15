"""
plot_results.py  —  CAAP Benchmark Visualisation
Usage:
    python scripts/plot_results.py
    python scripts/plot_results.py --main results/benchmark_main.csv \
                                   --params results/benchmark_params.csv \
                                   --output results/plots
"""

from __future__ import annotations

import argparse
import warnings
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import matplotlib.ticker as mticker
import numpy as np
import pandas as pd
import seaborn as sns

warnings.filterwarnings("ignore")

# ── colour / style ────────────────────────────────────────────────────────────
PALETTE = {
    "LinearProbing": "#e41a1c",
    "LocallyLinear": "#377eb8",
    "WalkFirst":     "#4daf4a",
    "AdaptiveLocal": "#ff7f00",
}
MARKERS = {
    "LinearProbing": "o",
    "LocallyLinear": "s",
    "WalkFirst":     "^",
    "AdaptiveLocal": "D",
}
ORDER = ["LinearProbing", "LocallyLinear", "WalkFirst", "AdaptiveLocal"]

sns.set_theme(style="whitegrid", font_scale=1.05)

# ── metric catalogue ──────────────────────────────────────────────────────────
METRICS = [
    ("avg_insert_probes",           "Média de sondas por inserção"),
    ("avg_search_success_probes",   "Média de sondas — busca com sucesso"),
    ("avg_search_fail_probes",      "Média de sondas — busca malsucedida"),
    ("max_cluster",                 "Maior cluster (média experimental)"),
    ("worst_insert_probes",         "Pior caso de sondas na inserção"),
    ("insert_ms",                   "Tempo de inserção (ms)"),
]

# ── helpers ───────────────────────────────────────────────────────────────────

def sem(series: pd.Series) -> float:
    n = series.count()
    return series.std() / np.sqrt(n) if n > 1 else 0.0


def aggregate_main(df: pd.DataFrame) -> pd.DataFrame:
    """Group by (strategy, load_factor, table_size) and compute stats."""
    agg = (
        df.groupby(["strategy", "load_factor", "table_size"])
        .agg(
            avg_insert_probes         =("avg_insert_probes",         "mean"),
            sem_insert_probes         =("avg_insert_probes",         sem),
            avg_search_success_probes =("avg_search_success_probes", "mean"),
            sem_search_success_probes =("avg_search_success_probes", sem),
            avg_search_fail_probes    =("avg_search_fail_probes",    "mean"),
            sem_search_fail_probes    =("avg_search_fail_probes",    sem),
            avg_max_cluster           =("max_cluster",               "mean"),
            sem_max_cluster           =("max_cluster",               sem),
            avg_worst_insert          =("worst_insert_probes",       "mean"),
            sem_worst_insert          =("worst_insert_probes",       sem),
            avg_insert_ms             =("insert_ms",                 "mean"),
            sem_insert_ms             =("insert_ms",                 sem),
        )
        .reset_index()
    )
    return agg


# ─────────────────────────────────────────────────────────────────────────────
#  1. Line plots with IC-95 error bars  (one subplot per table_size)
# ─────────────────────────────────────────────────────────────────────────────

METRIC_COLS = {
    "avg_insert_probes":         ("avg_insert_probes",         "sem_insert_probes"),
    "avg_search_success_probes": ("avg_search_success_probes", "sem_search_success_probes"),
    "avg_search_fail_probes":    ("avg_search_fail_probes",    "sem_search_fail_probes"),
    "max_cluster":               ("avg_max_cluster",           "sem_max_cluster"),
    "worst_insert_probes":       ("avg_worst_insert",          "sem_worst_insert"),
    "insert_ms":                 ("avg_insert_ms",             "sem_insert_ms"),
}

def plot_lines_with_ci(agg: pd.DataFrame, out_dir: Path) -> None:
    table_sizes = sorted(agg["table_size"].unique())
    n_sizes = len(table_sizes)

    for raw_col, label in METRICS:
        mean_col, sem_col = METRIC_COLS[raw_col]

        fig, axes = plt.subplots(1, n_sizes, figsize=(5 * n_sizes, 4.5), sharey=False)
        if n_sizes == 1:
            axes = [axes]

        for ax, tsize in zip(axes, table_sizes):
            sub = agg[agg["table_size"] == tsize]
            for strat in ORDER:
                s = sub[sub["strategy"] == strat].sort_values("load_factor")
                if s.empty:
                    continue
                x   = s["load_factor"].values
                y   = s[mean_col].values
                err = 1.96 * s[sem_col].values
                ax.errorbar(
                    x, y, yerr=err,
                    label=strat,
                    color=PALETTE.get(strat),
                    marker=MARKERS.get(strat, "o"),
                    markersize=5,
                    linewidth=1.6,
                    capsize=3,
                    capthick=1.2,
                )
            ax.set_title(f"n = {tsize:,}", fontsize=10)
            ax.set_xlabel("Fator de carga (α)")
            ax.xaxis.set_major_formatter(mticker.FormatStrFormatter("%.2f"))
            ax.tick_params(axis="x", rotation=30)

        axes[0].set_ylabel(label)
        handles, labels = axes[0].get_legend_handles_labels()
        fig.legend(handles, labels, loc="upper center",
                   ncol=len(ORDER), bbox_to_anchor=(0.5, 1.02), fontsize=9)
        fig.suptitle(f"{label}  [IC 95 %]", y=1.06, fontsize=11)
        fig.tight_layout()
        fname = out_dir / f"line_{raw_col}.png"
        fig.savefig(fname, dpi=150, bbox_inches="tight")
        plt.close(fig)
        print(f"  saved {fname.name}")


# ─────────────────────────────────────────────────────────────────────────────
#  2. Box-plots  (distribution over seeds at key load factors)
# ─────────────────────────────────────────────────────────────────────────────

BOXPLOT_ALPHAS = [0.70, 0.80, 0.90, 0.95, 0.97]

def plot_boxplots(df: pd.DataFrame, out_dir: Path) -> None:
    tsize_default = 65536
    df_sub = df[df["table_size"] == tsize_default].copy()
    df_sub["strategy"] = pd.Categorical(df_sub["strategy"], categories=ORDER, ordered=True)

    for alpha in BOXPLOT_ALPHAS:
        chunk = df_sub[np.isclose(df_sub["load_factor"], alpha)]
        if chunk.empty:
            continue

        fig, axes = plt.subplots(1, len(METRICS), figsize=(3.5 * len(METRICS), 4.5))
        for ax, (col, label) in zip(axes, METRICS):
            sns.boxplot(
                data=chunk,
                x="strategy", y=col,
                palette=PALETTE,
                order=ORDER,
                width=0.5,
                flierprops=dict(marker=".", markersize=3),
                ax=ax,
            )
            ax.set_title(label, fontsize=8)
            ax.set_xlabel("")
            ax.set_ylabel("")
            ax.tick_params(axis="x", rotation=30, labelsize=7)

        fig.suptitle(f"Distribuição (α = {alpha:.2f}, n = {tsize_default:,})", fontsize=11)
        fig.tight_layout()
        alpha_str = f"{alpha:.2f}".replace(".", "")
        fname = out_dir / f"boxplot_alpha{alpha_str}.png"
        fig.savefig(fname, dpi=150, bbox_inches="tight")
        plt.close(fig)
        print(f"  saved {fname.name}")


# ─────────────────────────────────────────────────────────────────────────────
#  3. Scalability plot  (table_size on x-axis, fixed α = 0.90)
# ─────────────────────────────────────────────────────────────────────────────

def plot_scalability(agg: pd.DataFrame, out_dir: Path) -> None:
    alpha_target = 0.90
    sub = agg[np.isclose(agg["load_factor"], alpha_target)]
    if sub.empty:
        return

    fig, axes = plt.subplots(1, len(METRICS), figsize=(3.5 * len(METRICS), 4.5))
    mean_cols = [mc for mc, _ in METRIC_COLS.values()]

    for ax, (raw_col, label) in zip(axes, METRICS):
        mc, sc = METRIC_COLS[raw_col]
        for strat in ORDER:
            s = sub[sub["strategy"] == strat].sort_values("table_size")
            if s.empty:
                continue
            ax.errorbar(
                s["table_size"].values,
                s[mc].values,
                yerr=1.96 * s[sc].values,
                label=strat,
                color=PALETTE.get(strat),
                marker=MARKERS.get(strat, "o"),
                markersize=5,
                linewidth=1.6,
                capsize=3,
            )
        ax.set_xscale("log", base=2)
        ax.set_xlabel("Tamanho da tabela ($\log_2$)")
        ax.set_title(label, fontsize=8)
        ax.tick_params(axis="x", rotation=20)

    axes[0].set_ylabel("Valor da métrica")
    handles, labels = axes[0].get_legend_handles_labels()
    fig.legend(handles, labels, loc="upper center",
               ncol=len(ORDER), bbox_to_anchor=(0.5, 1.02), fontsize=9)
    fig.suptitle(f"Escalabilidade (α = {alpha_target:.2f})", y=1.06, fontsize=11)
    fig.tight_layout()
    fname = out_dir / "scalability.png"
    fig.savefig(fname, dpi=150, bbox_inches="tight")
    plt.close(fig)
    print(f"  saved {fname.name}")


# ─────────────────────────────────────────────────────────────────────────────
#  4. Parameter sweep heatmaps  (cluster_threshold × block_fill_limit)
# ─────────────────────────────────────────────────────────────────────────────

HEATMAP_ALPHAS  = [0.80, 0.90, 0.95, 0.97]
HEATMAP_METRICS = [
    ("avg_insert_probes",       "Média de sondas — inserção"),
    ("max_cluster",             "Maior cluster médio"),
    ("avg_search_fail_probes",  "Média de sondas — busca malsucedida"),
]

def plot_heatmaps(df_params: pd.DataFrame, out_dir: Path) -> None:
    for alpha in HEATMAP_ALPHAS:
        chunk = df_params[np.isclose(df_params["load_factor"], alpha)]
        if chunk.empty:
            continue

        for col, label in HEATMAP_METRICS:
            pivot = (
                chunk.groupby(["cluster_threshold", "block_fill_limit"])[col]
                .mean()
                .reset_index()
                .pivot(index="cluster_threshold", columns="block_fill_limit", values=col)
            )
            if pivot.empty:
                continue

            # Find global minimum to annotate with ★
            min_val = pivot.values.min()
            min_pos = np.unravel_index(pivot.values.argmin(), pivot.shape)

            fig, ax = plt.subplots(figsize=(7, 5))
            sns.heatmap(
                pivot,
                ax=ax,
                cmap="YlOrRd",
                annot=True,
                fmt=".2f",
                linewidths=0.4,
                linecolor="grey",
                cbar_kws={"label": label},
            )
            # Mark best cell with a star
            ax.add_patch(plt.Rectangle(
                (min_pos[1], min_pos[0]), 1, 1,
                fill=False, edgecolor="blue", lw=2.5, label="mínimo global"
            ))

            best_ct = pivot.index[min_pos[0]]
            best_bf = pivot.columns[min_pos[1]]
            ax.set_title(
                f"{label}\nα = {alpha:.2f}  —  melhor: ct={best_ct}, bf={best_bf:.2f}  (val={min_val:.2f})",
                fontsize=9,
            )
            ax.set_xlabel("block_fill_limit")
            ax.set_ylabel("cluster_threshold")

            alpha_str = f"{alpha:.2f}".replace(".", "")
            fname = out_dir / f"heatmap_{col}_alpha{alpha_str}.png"
            fig.tight_layout()
            fig.savefig(fname, dpi=150, bbox_inches="tight")
            plt.close(fig)
            print(f"  saved {fname.name}")

    # Best-parameter summary table
    best_rows = []
    for alpha in HEATMAP_ALPHAS:
        chunk = df_params[np.isclose(df_params["load_factor"], alpha)]
        if chunk.empty:
            continue
        grp = (
            chunk.groupby(["cluster_threshold", "block_fill_limit"])
            ["avg_insert_probes"].mean()
        )
        best_idx = grp.idxmin()
        best_rows.append({
            "alpha": alpha,
            "best_ct": best_idx[0],
            "best_bf": best_idx[1],
            "avg_insert_probes": grp[best_idx],
        })
    if best_rows:
        df_best = pd.DataFrame(best_rows)
        path = out_dir.parent / "best_params.csv"
        df_best.to_csv(path, index=False, float_format="%.4f")
        print(f"\n  Best-params table saved: {path}")
        print(df_best.to_string(index=False))


# ─────────────────────────────────────────────────────────────────────────────
#  CLI entry point
# ─────────────────────────────────────────────────────────────────────────────

def main() -> None:
    parser = argparse.ArgumentParser(description="CAAP benchmark plots")
    parser.add_argument("--main",   default="results/benchmark_main.csv")
    parser.add_argument("--params", default="results/benchmark_params.csv")
    parser.add_argument("--output", default="results/plots")
    args = parser.parse_args()

    out_dir = Path(args.output)
    out_dir.mkdir(parents=True, exist_ok=True)

    # ── Main CSV ──────────────────────────────────────────────────────────────
    main_path = Path(args.main)
    if not main_path.exists():
        print(f"[warn] main CSV not found: {main_path}")
    else:
        print(f"\nLoading {main_path} …")
        df = pd.read_csv(main_path)
        agg = aggregate_main(df)
        agg.to_csv(out_dir.parent / "benchmark_summary.csv", index=False, float_format="%.6f")
        print(f"  summary saved → benchmark_summary.csv")

        print("\n— Line plots with IC-95 —")
        plot_lines_with_ci(agg, out_dir)

        print("\n— Box-plots —")
        plot_boxplots(df, out_dir)

        print("\n— Scalability —")
        plot_scalability(agg, out_dir)

    # ── Params CSV ────────────────────────────────────────────────────────────
    params_path = Path(args.params)
    if not params_path.exists():
        print(f"\n[info] params CSV not found ({params_path}), skipping heatmaps.")
    else:
        print(f"\nLoading {params_path} …")
        df_params = pd.read_csv(params_path)
        print("— Heatmaps —")
        plot_heatmaps(df_params, out_dir)

    print(f"\nAll plots written to {out_dir}/")


if __name__ == "__main__":
    main()
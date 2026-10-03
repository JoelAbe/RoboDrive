"""Analyse recorded RoboDrive runs.

Reads one or more run CSVs, prints a summary table, and writes plots
comparing wheel odometry and gyroscope dead reckoning against ground truth.

    python Tools/analysis/analyse_run.py Saved/RoboDrive/Runs/run_*.csv --out docs/images
"""

from __future__ import annotations

import argparse
from dataclasses import dataclass
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np
import pandas as pd


def read_metadata(path: Path) -> dict[str, str]:
    """Parse the leading '#' comment lines into key=value pairs.

    The recorder writes the settings that produced a run above the data, so a
    run can be reproduced and two runs can be compared knowing what differed.
    """
    metadata: dict[str, str] = {}
    with path.open(encoding="utf-8") as handle:
        for line in handle:
            if not line.startswith("#"):
                break
            for token in line.lstrip("# ").split():
                key, separator, value = token.partition("=")
                if separator:
                    metadata[key] = value
    return metadata


def read_run(path: Path) -> tuple[pd.DataFrame, dict[str, str]]:
    return pd.read_csv(path, comment="#"), read_metadata(path)


@dataclass(frozen=True)
class Summary:
    name: str
    duration_s: float
    distance_m: float
    final_drift_m: float
    drift_percent: float
    max_drift_m: float
    heading_error_deg: float
    gyro_heading_error_deg: float


def summarise(frame: pd.DataFrame, name: str) -> Summary:
    distance_m = float(frame["travelled_m"].iloc[-1])
    final_drift_m = float(frame["pos_error_m"].iloc[-1])

    return Summary(
        name=name,
        duration_s=float(frame["time_s"].iloc[-1] - frame["time_s"].iloc[0]),
        distance_m=distance_m,
        final_drift_m=final_drift_m,
        # Odometry error is quoted as a fraction of distance travelled, not as
        # an absolute number: drifting a metre over 10 m and over 1 km are very
        # different results.
        drift_percent=100.0 * final_drift_m / distance_m if distance_m > 0.01 else float("nan"),
        max_drift_m=float(frame["pos_error_m"].max()),
        heading_error_deg=float(np.degrees(frame["heading_error_rad"].iloc[-1])),
        gyro_heading_error_deg=float(np.degrees(frame["gyro_heading_error_rad"].iloc[-1])),
    )


def plot_paths(frame: pd.DataFrame, name: str, out_dir: Path) -> Path:
    """Ground truth against the odometry estimate, in the plane."""
    figure, axes = plt.subplots(figsize=(7, 7))

    axes.plot(frame["true_x_m"], frame["true_y_m"], label="ground truth", linewidth=2)
    axes.plot(frame["odom_x_m"], frame["odom_y_m"], label="wheel odometry", linewidth=2, linestyle="--")
    axes.plot(frame["true_x_m"].iloc[0], frame["true_y_m"].iloc[0], "o", markersize=9, label="start")

    # Equal aspect, or a drift of half a metre looks like a different shape
    # rather than a small error.
    axes.set_aspect("equal", adjustable="datalim")
    axes.set_xlabel("x (m)")
    axes.set_ylabel("y (m)")
    axes.set_title(f"Path: {name}")
    axes.grid(True, alpha=0.3)
    axes.legend()

    output = out_dir / f"{name}_paths.png"
    figure.savefig(output, dpi=150, bbox_inches="tight")
    plt.close(figure)
    return output


def plot_drift_vs_distance(runs: list[tuple[str, pd.DataFrame]], out_dir: Path) -> Path:
    """Position error against distance travelled, with reference slopes.

    Distance on the x axis rather than time, because odometry error is driven
    by how far the wheels turned, not by how long the robot was switched on.
    """
    figure, axes = plt.subplots(figsize=(9, 5.5))

    for name, frame in runs:
        axes.plot(frame["travelled_m"], frame["pos_error_m"], label=name, linewidth=2)

    furthest = max(float(frame["travelled_m"].max()) for _, frame in runs)
    reference = np.linspace(0.0, furthest, 50)
    for percent, style in ((1.0, ":"), (5.0, "-.")):
        axes.plot(reference, reference * percent / 100.0, style, color="grey",
                  linewidth=1, label=f"{percent:g}% of distance")

    axes.set_xlabel("distance travelled (m)")
    axes.set_ylabel("position error (m)")
    axes.set_title("Odometry drift against distance travelled")
    axes.grid(True, alpha=0.3)
    axes.legend()

    output = out_dir / "drift_vs_distance.png"
    figure.savefig(output, dpi=150, bbox_inches="tight")
    plt.close(figure)
    return output


def plot_heading_error(runs: list[tuple[str, pd.DataFrame]], out_dir: Path) -> Path:
    """Both dead reckoning sources' heading error, over time.

    Wheel odometry and the gyroscope fail differently, which is the argument
    for fusing them rather than trusting either.
    """
    figure, axes = plt.subplots(figsize=(9, 5.5))

    for name, frame in runs:
        axes.plot(frame["time_s"], np.degrees(frame["heading_error_rad"]),
                  label=f"{name}: wheel odometry", linewidth=2)
        axes.plot(frame["time_s"], np.degrees(frame["gyro_heading_error_rad"]),
                  label=f"{name}: gyroscope", linewidth=2, linestyle="--")

    axes.axhline(0.0, color="grey", linewidth=1)
    axes.set_xlabel("simulation time (s)")
    axes.set_ylabel("heading error (degrees)")
    axes.set_title("Heading error by dead reckoning source")
    axes.grid(True, alpha=0.3)
    axes.legend()

    output = out_dir / "heading_error.png"
    figure.savefig(output, dpi=150, bbox_inches="tight")
    plt.close(figure)
    return output


def print_summaries(summaries: list[Summary]) -> None:
    header = f"{'run':<28}{'time s':>9}{'dist m':>9}{'drift m':>9}{'drift %':>9}{'max m':>8}{'odom deg':>10}{'gyro deg':>10}"
    print(header)
    print("-" * len(header))
    for item in summaries:
        print(
            f"{item.name:<28}{item.duration_s:>9.1f}{item.distance_m:>9.1f}"
            f"{item.final_drift_m:>9.3f}{item.drift_percent:>9.2f}{item.max_drift_m:>8.3f}"
            f"{item.heading_error_deg:>10.1f}{item.gyro_heading_error_deg:>10.1f}"
        )


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("runs", nargs="+", type=Path, help="run CSV files")
    parser.add_argument("--out", type=Path, default=Path("docs/images"), help="where to write plots")
    args = parser.parse_args()

    args.out.mkdir(parents=True, exist_ok=True)

    loaded: list[tuple[str, pd.DataFrame]] = []
    for path in args.runs:
        frame, metadata = read_run(path)
        name = path.stem
        loaded.append((name, frame))

        radius_error = metadata.get("radius_error_factor", "?")
        noise = metadata.get("encoder_noise_rads", "?")
        print(f"{name}: {len(frame)} samples, radius_error_factor={radius_error}, encoder_noise={noise}")

    print()
    print_summaries([summarise(frame, name) for name, frame in loaded])
    print()

    for name, frame in loaded:
        print(f"wrote {plot_paths(frame, name, args.out)}")
    print(f"wrote {plot_drift_vs_distance(loaded, args.out)}")
    print(f"wrote {plot_heading_error(loaded, args.out)}")


if __name__ == "__main__":
    main()
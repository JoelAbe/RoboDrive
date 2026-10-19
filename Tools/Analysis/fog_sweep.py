"""Measure how fog affects the robot: run one scenario across visibilities.

    python Tools/Analysis/fog_sweep.py
    python Tools/Analysis/fog_sweep.py Scenarios/slalom.json --runs 5
    python Tools/Analysis/fog_sweep.py --visibilities 40 25

Every visibility is run twice over. Once with the lidar as configured, which
feels both effects of fog, and once with its fog echoes switched off, which
leaves only the loss of range. Comparing the two shows which effect causes a
failure. Fog echoes are random, so each case is run several times.

Ends by printing a Markdown table for the write-up in docs/fog.md. The fog of
the last case is left in place; load any scenario to clear it.

The simulation must be playing.
"""

from __future__ import annotations

import argparse
import copy
import json
import math
import statistics
import sys
from pathlib import Path

from robodrive_client import DEFAULT_HOST, DEFAULT_PORT, SimClient
from run_scenarios import Outcome, run_scenario_data

# Thinning fog, from light haze to dense fog. Clear air is always run first,
# as the baseline every other row is compared with.
DEFAULT_VISIBILITIES = [100.0, 50.0, 30.0, 20.0, 15.0, 10.0]


def detection_range(range_max: float, visibility: float | None) -> float:
    """How far the lidar can still see a surface in this fog, m.

    Solves Range * exp(Extinction * Range) = RangeMax, the boundary of
    RoboFog::IsDetectable. There is no neat formula for that, so it is found
    by bisection: keep halving an interval known to contain the answer.
    """
    if visibility is None:
        return range_max

    extinction = math.log(50.0) / visibility
    low, high = 0.0, range_max
    for _ in range(50):
        middle = 0.5 * (low + high)
        if middle * math.exp(extinction * middle) <= range_max:
            low = middle
        else:
            high = middle
    return low


def describe(visibility: float | None) -> str:
    return "clear" if visibility is None else f"{visibility:g} m"


def summarise(outcomes: list[Outcome]) -> str:
    """How many runs reached the goal, and their mean time, e.g. '3/3, 24.8 s'."""
    if not outcomes:
        return "-"

    passed = [outcome for outcome in outcomes if outcome.passed]
    summary = f"{len(passed)}/{len(outcomes)}"
    if passed:
        summary += f", {statistics.mean(outcome.elapsed_s for outcome in passed):.1f} s"
    return summary


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("scenario", nargs="?", type=Path, default=Path("Scenarios/slalom.json"),
                        help="the scenario to run; defaults to the slalom")
    parser.add_argument("--visibilities", nargs="+", type=float, default=DEFAULT_VISIBILITIES,
                        help="visibilities to test, m")
    parser.add_argument("--runs", type=int, default=3, help="runs per case")
    parser.add_argument("--without-obstacles", action="store_true", help="remove the scenario's obstacles, leaving only the fog")
    parser.add_argument("--host", default=DEFAULT_HOST)
    parser.add_argument("--port", type=int, default=DEFAULT_PORT)
    args = parser.parse_args()

    base = json.loads(args.scenario.read_text(encoding="utf-8"))

    # The sweep sets its own fog, and judges plain pass or fail rather than
    # against any expectation the file carries.
    base.pop("visibility_m", None)
    base.pop("known_failure", None)
    # With nothing solid to hit, anything that still goes wrong is the fog's
    # doing alone.
    if args.without_obstacles:
        base["obstacles"] = []

    cases: list[float | None] = [None] + args.visibilities
    rows: list[tuple[float | None, list[Outcome], list[Outcome]]] = []

    with SimClient(args.host, args.port) as client:
        range_max = client.call("get_sensors")["lidar"]["range_max"]
        configured = client.call("set_lidar")["fog_echo_fraction"]

        def run_case(scenario: dict, echo_fraction: float, label: str) -> list[Outcome]:
            client.call("set_lidar", fog_echo_fraction=echo_fraction)
            outcomes = []
            for run in range(args.runs):
                outcome = run_scenario_data(client, scenario, "sweep", "robot")
                outcomes.append(outcome)
                print(f"{describe(scenario.get('visibility_m')):>6}  {label:<11} "
                      f"run {run + 1}: {outcome.detail}", flush=True)
            return outcomes

        try:
            for visibility in cases:
                scenario = copy.deepcopy(base)
                if visibility is not None:
                    scenario["visibility_m"] = visibility

                with_echoes = run_case(scenario, configured, "echoes on")

                # In clear air there are no echoes to switch off, so the
                # second half would only repeat the first.
                without_echoes = [] if visibility is None else run_case(scenario, 0.0, "echoes off")

                rows.append((visibility, with_echoes, without_echoes))
        finally:
            # Leave the sensor as it was found, even if a run failed partway.
            client.call("set_lidar", fog_echo_fraction=configured)

    print()
    variant = " without obstacles" if args.without_obstacles else ""
    print(f"Scenario: {base.get('name', args.scenario.stem)}{variant}, {args.runs} runs per case, "
          f"fog echo fraction {configured:g}. Cells: runs that reached the goal, mean time.")
    print()
    print("| Visibility | Lidar sees to | Fog echoes on | Fog echoes off |")
    print("|---|---|---|---|")
    for visibility, with_echoes, without_echoes in rows:
        print(f"| {describe(visibility)} | {detection_range(range_max, visibility):.1f} m "
              f"| {summarise(with_echoes)} | {summarise(without_echoes)} |")

    return 0


if __name__ == "__main__":
    sys.exit(main())
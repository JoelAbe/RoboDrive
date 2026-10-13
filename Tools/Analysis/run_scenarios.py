"""Run scenarios against the simulation and report pass or fail.

    python Tools/analysis/run_scenarios.py
    python Tools/analysis/run_scenarios.py Scenarios/corridor.json

Each scenario is loaded, driven toward its goal by a simple controller that
steers by odometry alone, and then judged against ground truth. Exits with 0
if every scenario behaved as expected, and 1 otherwise, so CI can read the
result directly.

The simulation must be playing.
"""

from __future__ import annotations

import argparse
import json
import math
import sys
from dataclasses import dataclass
from pathlib import Path

from robodrive_client import DEFAULT_HOST, DEFAULT_PORT, SimClient

# Control runs at 10 Hz: a command, then 12 steps of the 120 Hz simulation.
STEPS_PER_COMMAND = 12

# A gentle controller. These are deliberately well inside the robot's limits,
# so the wheels never saturate and a failure means a real problem rather than
# the controller asking for more grip than exists.
MAX_SPEED_MS = 0.5
MAX_TURN_RADS = 1.0
HEADING_GAIN = 1.5
SPEED_GAIN = 0.5

# How long to hold an explicit stop before judging, in simulation steps.
SETTLE_STEPS = 60


def wrap_angle(angle: float) -> float:
    """Wrap to (-pi, pi], so turning left by 350 degrees becomes right by 10."""
    return (angle + math.pi) % (2.0 * math.pi) - math.pi


@dataclass(frozen=True)
class Outcome:
    name: str
    passed: bool
    known_failure: str | None
    detail: str

    @property
    def status(self) -> str:
        if self.known_failure:
            return "XPASS" if self.passed else "XFAIL"
        return "PASS" if self.passed else "FAIL"

    @property
    def fails_run(self) -> bool:
        # A real failure fails the run. So does a known failure that has
        # started passing: the marker should be removed so the scenario
        # begins protecting the capability that now exists.
        return self.status in ("FAIL", "XPASS")


def go_to_goal(pose: dict, goal: dict) -> tuple[float, float, float]:
    """Steer toward the goal using the robot's own odometry.

    Uses no ground truth. That is the whole point: a controller that steered by
    the truth would hide the dead reckoning error the test is meant to catch.

    Returns (linear velocity, angular velocity, believed distance to goal).
    """
    dx = goal["x"] - pose["x"]
    dy = goal["y"] - pose["y"]
    distance = math.hypot(dx, dy)

    heading_error = wrap_angle(math.atan2(dy, dx) - pose["yaw"])

    angular = max(-MAX_TURN_RADS, min(MAX_TURN_RADS, HEADING_GAIN * heading_error))

    # Drive forward only when roughly facing the goal. cos(error) is 1 when
    # facing it, 0 when it is off to the side, and negative behind, which the
    # max() clamps to zero: the robot turns on the spot rather than reversing.
    linear = min(MAX_SPEED_MS, SPEED_GAIN * distance) * max(0.0, math.cos(heading_error))

    return linear, angular, distance


def run_scenario(client: SimClient, path: Path) -> Outcome:
    scenario = json.loads(path.read_text(encoding="utf-8"))
    name = scenario.get("name", path.stem)
    known_failure = scenario.get("known_failure")

    if "goal" not in scenario:
        return Outcome(name, False, known_failure, "scenario has no goal to judge against")

    goal = scenario["goal"]
    tolerance = goal.get("tolerance_m", 0.5)
    time_limit = scenario.get("time_limit_s", 60.0)

    client.call("load_scenario", scenario=scenario)
    start_time = client.call("get_state")["sim_time"]

    believes_arrived = False
    elapsed = 0.0

    while elapsed <= time_limit:
        sensors = client.call("get_sensors", include_lidar=False)
        elapsed = sensors["sim_time"] - start_time

        linear, angular, believed_distance = go_to_goal(sensors["odometry"], goal)

        # Stop when odometry says we are well inside the tolerance. Whether we
        # really are is decided below, by the truth.
        if believed_distance < 0.5 * tolerance:
            believes_arrived = True
            break

        client.call("set_command", v=linear, w=angular)
        client.call("step", steps=STEPS_PER_COMMAND)

    # Hold an explicit stop while the robot settles. Simply going quiet is not
    # enough: the command would expire and any lower-priority source, like the
    # stub autonomy, would take over and drive off before we could measure.
    for _ in range(SETTLE_STEPS // STEPS_PER_COMMAND):
        client.call("set_command", v=0.0, w=0.0)
        client.call("step", steps=STEPS_PER_COMMAND)

    truth = client.call("get_ground_truth")
    true_pose = truth["pose"]
    true_distance = math.hypot(goal["x"] - true_pose["x"], goal["y"] - true_pose["y"])
    odometry_error = truth.get("odometry_error_m", float("nan"))

    if not believes_arrived:
        return Outcome(name, False, known_failure,
                       f"timed out after {time_limit:.0f} s, {true_distance:.2f} m from goal")

    if true_distance > tolerance:
        # The interesting failure: odometry was confident and wrong.
        return Outcome(name, False, known_failure,
                       f"believed it arrived but was {true_distance:.2f} m from goal "
                       f"(odometry error {odometry_error:.2f} m)")

    return Outcome(name, True, known_failure,
                   f"reached goal {true_distance:.2f} m off in {elapsed:.1f} s "
                   f"(odometry error {odometry_error:.2f} m)")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("scenarios", nargs="*", type=Path,
                        help="scenario files; defaults to every file in Scenarios/")
    parser.add_argument("--host", default=DEFAULT_HOST)
    parser.add_argument("--port", type=int, default=DEFAULT_PORT)
    args = parser.parse_args()

    paths = args.scenarios or sorted(Path("Scenarios").glob("*.json"))
    if not paths:
        print("no scenarios found", file=sys.stderr)
        return 1

    outcomes: list[Outcome] = []

    with SimClient(args.host, args.port) as client:
        for path in paths:
            outcome = run_scenario(client, path)
            outcomes.append(outcome)

            line = f"{outcome.name:<16}{outcome.status:<7}{outcome.detail}"
            if outcome.known_failure:
                line += f"\n{'':<23}known failure: {outcome.known_failure}"
            print(line)

    failed = [outcome for outcome in outcomes if outcome.fails_run]

    print()
    if failed:
        for outcome in failed:
            if outcome.status == "XPASS":
                print(f"{outcome.name}: passed but is marked known_failure. "
                      "Remove the marker so it protects the new behaviour.")
        print(f"{len(failed)} of {len(outcomes)} scenarios failed")
        return 1

    print(f"all {len(outcomes)} scenarios behaved as expected")
    return 0


if __name__ == "__main__":
    sys.exit(main())
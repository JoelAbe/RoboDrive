"""Run scenarios against the simulation and report pass or fail.

    python Tools/Analysis/run_scenarios.py
    python Tools/Analysis/run_scenarios.py --driver harness
    python Tools/Analysis/run_scenarios.py Scenarios/corridor.json

Loading a scenario gives the robot its goal. By default the robot then drives
itself with its in-engine brain and this script only watches. With
--driver harness, a simple controller in this script drives instead, as a
baseline: if a scenario fails under the robot's brain but passes under the
harness, the problem is in the brain rather than the world.

Either way the result is judged against ground truth. Exits with 0 if every
scenario behaved as expected and 1 otherwise, so CI can read the result.

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

# Control and observation run at 10 Hz: 12 steps of the 120 Hz simulation.
STEPS_PER_COMMAND = 12

# The harness controller. Deliberately well inside the robot's limits, so a
# failure means a real problem rather than asking for more grip than exists.
MAX_SPEED_MS = 0.5
MAX_TURN_RADS = 1.0
HEADING_GAIN = 1.5
SPEED_GAIN = 0.5

# How long to let the robot come to rest before judging, in simulation steps.
SETTLE_STEPS = 60

DRIVERS = ("robot", "harness")


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
    """The harness controller: steer toward the goal by odometry alone.

    Returns (linear velocity, angular velocity, believed distance to goal).
    """
    dx = goal["x"] - pose["x"]
    dy = goal["y"] - pose["y"]
    distance = math.hypot(dx, dy)

    heading_error = wrap_angle(math.atan2(dy, dx) - pose["yaw"])
    angular = max(-MAX_TURN_RADS, min(MAX_TURN_RADS, HEADING_GAIN * heading_error))
    linear = min(MAX_SPEED_MS, SPEED_GAIN * distance) * max(0.0, math.cos(heading_error))

    return linear, angular, distance


def drive_with_harness(client: SimClient, goal: dict, tolerance: float,
                       time_limit: float, start_time: float) -> tuple[str, float]:
    """Steer from this script. Returns (how it ended, elapsed seconds)."""
    elapsed = 0.0
    while elapsed <= time_limit:
        sensors = client.call("get_sensors", include_lidar=False)
        elapsed = sensors["sim_time"] - start_time

        linear, angular, believed_distance = go_to_goal(sensors["odometry"], goal)
        if believed_distance < 0.5 * tolerance:
            return "arrived", elapsed

        client.call("set_command", v=linear, w=angular)
        client.call("step", steps=STEPS_PER_COMMAND)

    return "timeout", elapsed


def drive_with_robot(client: SimClient, time_limit: float,
                     start_time: float) -> tuple[str, float]:
    """Hands off: let the robot's own brain drive, and watch the mission.

    Sends no commands at all. A command from here would arrive as External,
    which outranks the robot's Autonomy source, and would take the wheel away
    from the very brain under test.
    """
    elapsed = 0.0
    while elapsed <= time_limit:
        client.call("step", steps=STEPS_PER_COMMAND)
        state = client.call("get_state")
        elapsed = state["sim_time"] - start_time

        status = state.get("mission", {}).get("status")
        if status in ("arrived", "failed"):
            return status, elapsed

    return "timeout", elapsed


def settle(client: SimClient, driver: str) -> None:
    """Let the robot come to rest before judging."""
    for _ in range(SETTLE_STEPS // STEPS_PER_COMMAND):
        # Under the harness, hold an explicit stop. Going quiet would let the
        # command expire and hand control to the robot's brain, which has the
        # same goal and would carry on driving before we could measure.
        # Under the robot, its brain already holds still once arrived.
        if driver == "harness":
            client.call("set_command", v=0.0, w=0.0)
        client.call("step", steps=STEPS_PER_COMMAND)


def run_scenario(client: SimClient, path: Path, driver: str) -> Outcome:
    scenario = json.loads(path.read_text(encoding="utf-8"))
    name = scenario.get("name", path.stem)
    known_failure = scenario.get("known_failure")

    if "goal" not in scenario:
        return Outcome(name, False, known_failure, "scenario has no goal to judge against")

    goal = scenario["goal"]
    tolerance = goal.get("tolerance_m", 0.5)
    time_limit = scenario.get("time_limit_s", 60.0)

    # Loading also sets the mission goal, which is how the robot's brain learns
    # where to go.
    client.call("load_scenario", scenario=scenario)
    start_time = client.call("get_state")["sim_time"]

    if driver == "harness":
        ending, elapsed = drive_with_harness(client, goal, tolerance, time_limit, start_time)
    else:
        ending, elapsed = drive_with_robot(client, time_limit, start_time)

    settle(client, driver)

    truth = client.call("get_ground_truth")
    true_pose = truth["pose"]
    true_distance = math.hypot(goal["x"] - true_pose["x"], goal["y"] - true_pose["y"])
    odometry_error = truth.get("odometry_error_m", float("nan"))

    if ending == "timeout":
        return Outcome(name, False, known_failure,
                       f"timed out after {time_limit:.0f} s, {true_distance:.2f} m from goal")

    if ending == "failed":
        return Outcome(name, False, known_failure,
                       f"the robot gave up after {elapsed:.1f} s, {true_distance:.2f} m from goal")

    if true_distance > tolerance:
        # The interesting failure: odometry was confident and wrong.
        return Outcome(name, False, known_failure,
                       f"believed it arrived but was {true_distance:.2f} m from goal "
                       f"(odometry error {odometry_error:.2f} m)")

    return Outcome(name, True, known_failure,
                   f"reached goal {true_distance:.2f} m off in {elapsed:.1f} s "
                   f"(odometry error {odometry_error:.2f} m)")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("scenarios", nargs="*", type=Path,
                        help="scenario files; defaults to every file in Scenarios/")
    parser.add_argument("--driver", choices=DRIVERS, default="robot",
                        help="who drives: the robot's own brain, or this script")
    parser.add_argument("--host", default=DEFAULT_HOST)
    parser.add_argument("--port", type=int, default=DEFAULT_PORT)
    args = parser.parse_args()

    paths = args.scenarios or sorted(Path("Scenarios").glob("*.json"))
    if not paths:
        print("no scenarios found", file=sys.stderr)
        return 1

    print(f"driver: {args.driver}\n")
    outcomes: list[Outcome] = []

    with SimClient(args.host, args.port) as client:
        for path in paths:
            outcome = run_scenario(client, path, args.driver)
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
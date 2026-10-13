# Scenario files

A scenario describes one test: where the robot starts, what is in its way,
where it should get to, and how long it has. Scenarios live in `Scenarios/`
at the repository root as JSON, so they can be reviewed in a diff and written
without Unreal installed.

## Who reads what

A scenario is read by two different programs, and each reads only its own
fields.

- **The simulator** builds the world: it places obstacles, moves the robot to
  its start pose and marks the goal. It ignores every other field.
- **The test harness** (`Tools/Analysis/run_scenarios.py`) drives the robot and
  judges the result against ground truth, using the goal and time limit.

Keeping the pass and fail criteria out of the simulator means the rules a test
is judged by are in the same place as the test, in a language the person
writing it already uses.

## Coordinates

Everything is in REP-103 conventions, the same as the rest of the project:
metres and radians, +X forward, **+Y to the left**, yaw positive
counter-clockwise. The origin is the world origin of the level.

## Format

```json
{
  "schema_version": 1,
  "name": "corridor",
  "description": "Drive 8 m straight down a corridor of boxes.",
  "start": { "x": 0.0, "y": 0.0, "yaw": 0.0 },
  "goal": { "x": 8.0, "y": 0.0, "tolerance_m": 0.5 },
  "time_limit_s": 40.0,
  "obstacles": [
    { "x": 2.0, "y": 1.0, "yaw": 0.0, "size": [1.0, 0.3, 0.6] }
  ]
}
```

| Field | Read by | Required | Meaning |
|---|---|---|---|
| `schema_version` | simulator | yes | Format version. Only `1` is accepted; anything else is refused rather than half-understood. |
| `name` | both | no | Short identifier used in reports. Defaults to the file name. |
| `description` | people | no | What the scenario tests and why. |
| `start` | simulator | yes | Robot start pose: `x`, `y`, optional `yaw`. |
| `obstacles` | simulator | no | Boxes to place. Each needs `x`, `y` and a three-element `size` in metres (length, width, height); `yaw` is optional. Boxes sit on the floor. |
| `goal` | harness, drawn by simulator | yes for the harness | Target position and `tolerance_m`, the radius within which the robot counts as having arrived. |
| `time_limit_s` | harness | no | Simulation seconds allowed. Defaults to 60. |
| `known_failure` | harness | no | A reason this scenario is expected to fail today. See below. |

## Known failures

A scenario can describe something the robot cannot do yet. Marking it with
`known_failure` and a reason keeps it in the suite without turning every run
red:

```json
"known_failure": "Needs lidar-based obstacle avoidance, which arrives in Phase 4."
```

The harness reports such a scenario as `XFAIL` when it fails, and does not
count it as a failure. If it unexpectedly *passes*, it is reported as `XPASS`
and **does** fail the run: the capability now exists, so the marker should be
removed and the scenario should start protecting it.

## Validation

The simulator checks the whole scenario before changing anything. A malformed
file is rejected with an error naming the problem, and the previous scenario
stays standing rather than being half-replaced.

## Obstacles and the robot's wheels

The robot's wheels are raycasts, not collision shapes (see
[ADR 0004](decisions/0004-raycast-wheel-vehicle-model.md)). Only the chassis
collides, so an obstacle that only overlaps where a wheel would be will be
driven straight through. An obstacle meant to block the robot must overlap the
chassis, which is 0.4 m wide.

## Running

With the simulation playing:

```
python Tools/Analysis/load_scenario.py Scenarios/corridor.json
python Tools/Analysis/run_scenarios.py
```

The first only builds the world, for looking at. The second runs every
scenario and exits non-zero if any fails.

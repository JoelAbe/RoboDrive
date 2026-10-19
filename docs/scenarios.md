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
- **The test harness** (`Tools/Analysis/run_scenarios.py`) judges the result
  against ground truth, using the goal and time limit.

Loading a scenario also gives the robot its goal, through the mission. By
default the robot's in-engine brain then drives and the harness only watches,
sending no commands of its own: a command from the harness would outrank the
brain and take the wheel away from the thing under test.

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
| `visibility_m` | simulator | no | Fog, as meteorological visibility in metres. Absent means clear air. See below. |
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

## Fog

`visibility_m` is the distance at which a dark object can no longer be made
out, as a weather report gives it. Fog affects only the lidar, in two ways:

- **Range loss.** Fog scatters light on the way out and back, so returns from
  distant surfaces become too weak to detect. At 10 m visibility the 12 m lidar
  sees about 3.3 m.
- **Clutter.** Some scattered light comes straight back, so the lidar reports
  returns from the fog itself. How readily it does so is a property of the
  sensor, the lidar's **Fog Echo Fraction**.

The level does not look foggy: nothing here changes rendering. In play, fog
echoes are drawn in cyan and returns from real surfaces in orange. The brain
receives both as identical ranges, since a real sensor cannot tell them apart.

`slalom_fog` is the slalom with 10 m visibility and nothing else changed. It is
a known failure: avoidance treats fog echoes as obstacles and steers around the
fog itself. How the robot fails across a range of visibilities, and why, is
measured in [fog.md](fog.md).

## What a pass does not check

A scenario passes if the robot ends within the goal tolerance, by ground truth,
inside the time limit. **It does not check whether the robot touched anything
on the way.** A robot that scraped a post and carried on would still pass.
Detecting that needs collision events reported through the API and a
criterion in the scenario format, neither of which exists yet.

Nor does it check the route. The slalom is passed by reaching the goal, whether
the robot weaves between the posts or goes round all of them on one side.

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

The first only builds the world, for looking at; the robot will drive itself
toward the goal. The second runs every scenario and exits non-zero if any
fails.

`run_scenarios.py --driver harness` drives with a simple controller in the
script instead of the robot's brain. It is a baseline: a scenario that fails
under the brain but passes under the harness points at the brain rather than
the world. To run the whole suite headless,
as CI does, use `./Tools/RunScenarios.ps1`.

The operations these scripts use are described in the [API reference](api.md).

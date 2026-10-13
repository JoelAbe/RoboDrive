# 0008. Define scenarios as JSON, judged by the test harness

- **Status:** Accepted

## Context

A scenario test needs a repeatable world and a rule for passing. Both should be
writable by someone who does not use Unreal, and reviewable when they change.

Unreal's own option is a Data Asset: editable in the Details panel, but stored
as a binary `.uasset` that cannot be diffed and cannot be written without the
editor.

## Decision

Scenarios are JSON files in `Scenarios/` at the repository root.

- **The simulator builds the world** from the start pose, obstacles and goal
  marker, and ignores every other field.
- **The test harness judges the result** from the goal and time limit, using
  ground truth as the oracle.
- The client sends the scenario's **contents** over the API, never a path.
- Every file carries a `schema_version`, and anything other than version 1 is
  refused.
- The simulator **validates the whole scenario before changing anything**.
- A scenario can be marked `known_failure`. It then reports `XFAIL` without
  failing the run, and fails the run if it unexpectedly passes.

## Consequences

- Scenarios can be reviewed in a pull request and written in any text editor.
- The rules a test is judged by sit in the same file as the test, readable by
  whoever writes it, rather than in C++.
- Because the simulator ignores fields it does not read, the format can grow
  for the harness without rebuilding the simulator.
- Sending contents rather than paths means client and simulator need not share
  a filesystem, which matters once the client runs in a container or CI runs
  elsewhere.
- A malformed scenario leaves the previous world standing rather than
  half-replaced.
- A strict known-failure rule means a scenario written for a capability that
  does not exist yet will turn the run red the day the capability arrives, so
  the marker gets removed and the scenario starts protecting it.
- Obstacles are boxes only. The robot's wheels are raycasts with no collision
  shape, so an obstacle must overlap the 0.4 m chassis to block it
  ([ADR 0004](0004-raycast-wheel-vehicle-model.md)).
- JSON has no comments, so the reasoning behind a scenario belongs in its
  `description` field.

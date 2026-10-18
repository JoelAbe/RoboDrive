# 0009. Give brains a sensor snapshot, never the robot

- **Status:** Accepted

## Context

A brain turns what the robot senses into a drive command. The simplest way to
give it that information is a pointer to the robot, from which it can read any
sensor it likes.

That pointer also leads to ground truth. The robot actor's transform is where
the robot really is, and a brain that can reach it will eventually read it:
for a debug check, a quick fix, a tuning experiment. It then works perfectly in
simulation and fails on hardware, where no such pointer exists.

## Decision

A brain receives an `FBrainInputs` snapshot and nothing else. The snapshot
holds what the robot could know: its odometry estimate, its latest lidar scan,
the simulation time and the current mission goal. It is passed by value.

Brains implement `FRoboBrain`, a plain C++ interface with a single `Update`
that takes the snapshot and returns a drive command and a mission status.
Brains live in `RoboAutonomy`; the component that builds snapshots from the
robot's sensors is the only code that touches the robot on a brain's behalf.

## Consequences

- A brain has no route to ground truth, so it cannot cheat even by accident.
  This is enforced by what it is given rather than by convention, in the same
  way the external API separates `get_sensors` from `get_ground_truth`.
- Brains can be unit tested by constructing a snapshot by hand: a goal to the
  left, an obstacle ahead. No world, actor or engine is needed.
- Because the snapshot is a copy, it cannot change while a brain is deciding.
  Copying a lidar scan at the brain's update rate is cheap at the rates in use.
- What a brain knows is written down in one struct. Giving brains a new sensor,
  such as the IMU, means extending the snapshot deliberately rather than
  reaching for it.
- An external brain using `get_sensors` sees the same information an in-engine
  brain does, so the two are comparable on equal terms.

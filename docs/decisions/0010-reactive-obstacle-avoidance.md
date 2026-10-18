# 0010. Avoid obstacles reactively, layered on go-to-goal

- **Status:** Accepted

## Context

Go-to-goal drives straight at the goal and knows nothing about obstacles. On
the slalom it pushed against the first post while odometry, which counts wheel
rotation, kept reporting progress, until the brain announced it had arrived
seven metres short of the goal.

The robot needs to get around obstacles it can see. The options range from
reactive methods, which decide from the current scan alone, to planners that
build a map and search it for a path. A planner needs localisation good enough
to place what it has seen on a map, and this robot has only dead reckoning.

## Decision

Avoidance is **reactive**: a simplified Vector Field Histogram. Each candidate
heading is tested by laying a corridor the robot's width plus a margin along
it, for a lookahead distance, and checking whether any scan point falls
inside. Candidates are searched outward from the goal's bearing, alternating
sides, so the clear heading that deviates least from the goal wins.

The avoiding brain **contains** a go-to-goal brain rather than inheriting from
one. Go-to-goal still decides whether there is a goal and whether the robot has
arrived; avoidance only changes which way the robot steers while it travels.
Both brains steer toward a heading through the same shared function.

When nothing is clear, the robot stops driving forward and turns toward the
goal's side to look for a gap. If it stays boxed in past a timeout, it reports
the mission **failed**.

## Consequences

- With no obstacle in view the avoiding brain behaves exactly like go-to-goal.
  A unit test compares the two directly, so a change to avoidance that alters
  open-space driving is caught.
- The goal's bearing comes from odometry and obstacles come from the lidar, and
  both are relative to the robot. Odometry drift can send the robot to the
  wrong place, but cannot make it misjudge an obstacle in front of it.
- There is no memory. The robot can be trapped by shapes a reactive method
  cannot escape, such as a U-shaped dead end facing the goal, where every
  heading toward the goal is blocked and the way out leads away from it. A
  planner on a map would be the next step, and needs better localisation first.
- Being boxed in ends in an honest `failed` rather than a false `arrived`. The
  harness reports it as the robot giving up.
- The search prefers the smallest deviation, so the route is whatever needs
  least turning, not a deliberate weave. Scenarios judge whether the robot
  reaches the goal, not the route it takes.
- A heading in the lidar's blind spot behind the robot is never chosen, since
  unknown is not clear.
- A beam with no return is treated as clear. In this simulation every surface
  reflects, so that holds; on hardware, glass and matte black surfaces also
  return nothing, and that assumption would need revisiting.
- The brain reports the heading it chose, and the component draws it, so a
  person watching can tell a wrong decision from bad steering toward a right
  one.

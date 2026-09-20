# 0003. Command the robot with (linear velocity, angular velocity)

- **Status:** Accepted

## Context

Something has to define what crosses the boundary between whatever is driving
the robot and the robot itself. The obvious alternative is wheel speeds, since
that is what the vehicle ultimately applies.

Several different sources will drive the same robot: keyboard input, an
in-engine autonomy, external test scripts, and eventually a ROS 2 stack. The
project also intends to support other steering geometries later, such as
skid-steer, for comparing how they degrade wheel odometry.

## Decision

The command is a body velocity: linear velocity in m/s (positive forward) and
angular velocity in rad/s (positive turning left). The vehicle converts that
into whatever its own wheels need, using the kinematics in `RoboDiffDrive.h`.

## Consequences

- A planner can express intent directly, without knowing how many wheels the
  robot has or how they are arranged.
- The same command works for other steering geometries. An Ackermann vehicle
  would apply its own turning-radius limit rather than requiring a different
  interface.
- It matches the ROS `geometry_msgs/Twist` convention (`linear.x`,
  `angular.z`), so the Phase 6 bridge needs no translation beyond transport.
- A command that a given vehicle cannot physically execute, such as a spin in
  place requested of an Ackermann vehicle, has to be clamped by the vehicle
  rather than rejected at the interface.
- Wheel-level control, such as commanding one wheel directly for a slip test,
  is not expressible through this interface and would need a separate path.

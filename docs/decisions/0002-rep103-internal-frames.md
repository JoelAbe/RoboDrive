# 0002. Use REP-103 frames internally

- **Status:** Accepted

## Context

Unreal works in centimetres with +Y to the right (left-handed), and positive
yaw turns clockwise seen from above, measured in degrees. Robotics literature
and ROS use REP-103: metres, +Y to the left (right-handed), positive yaw turns
left, measured in radians.

Published formulas assume REP-103. Differential-drive angular velocity,
`omega = (v_right - v_left) / L`, is positive for a left turn only in a
right-handed frame. Mixing the two conventions does not make results slightly
inaccurate; it mirrors them, and the degree/radian difference exaggerates every
turn by a factor of about 57.

## Decision

All maths in RoboSim and RoboAutonomy uses REP-103 and SI units. Conversion
happens only at the boundary with the engine — physics, transforms, debug
drawing — through `Source/RoboSim/Public/RoboFrames.h`.

- Positions and linear velocities: mirror Y, scale by 0.01.
- Yaw: change sign, convert degrees to radians.
- Full rotations and angular velocity follow a different rule, because they
  reverse under a mirror rather than simply flipping an axis. They are added
  with the IMU in Phase 2, when there is a concrete use for them.

## Consequences

- Published formulas and ROS message conventions apply as written, and a future
  ROS bridge needs no unit conversion of its own.
- Noise parameters are expressed in physically meaningful units, such as a
  lidar standard deviation of 0.02 m rather than 2 cm.
- Every crossing of the engine boundary must convert. A missed conversion is a
  silent bug that mirrors the result rather than producing an obvious error.
  This is mitigated by keeping every conversion in one tested file, including a
  test that checks the rules against Unreal's own rotation maths rather than
  against hand-derived values.
- Values shown in the editor's Details panel use Unreal's convention and need
  converting mentally when debugging.

*Note: the rotation and angular velocity conversions deferred above were
implemented in Phase 2 alongside the IMU.*

# 0017. Bridge to ROS 2 as an API client, natively on Windows

- **Status:** Accepted

## Context

Phase 6 connects the simulation to ROS 2: sensor topics and `/clock` out,
`/cmd_vel` in, with a ROS node driving the robot on simulation time.

The ways to do that differ in where ROS lives:

- **Inside Unreal**, with a plugin that links the ROS 2 client libraries into
  the engine (rclUE and similar). Messages leave without an extra hop, but the
  simulation then builds against ROS, which breaks the rule that `RoboSim`
  knows nothing about what drives it ([ADR 0001](0001-three-module-split.md)),
  and ties the engine build to one ROS distribution.
- **Outside, as a client of the API**, which already exists for exactly this:
  REP-103 units, simulation timestamps, a command watchdog, and sensors kept
  apart from ground truth ([ADR 0007](0007-external-api-module.md)). The cost
  is a hop through JSON over TCP, and the API's single connection.

The roadmap planned the second, in Docker. On the development machine neither
Docker nor WSL2 can be installed: both need Windows' Virtual Machine Platform,
and Windows' servicing store cannot add it or repair itself. ROS 2 also runs
natively on Windows, from a binary release with its dependencies provided by
pixi. Jazzy, the previous long-term release, supports only Windows 10 and
stopped shipping Windows builds when Windows 10 reached end of life; Lyrical,
the current one, supports Windows 11.

## Decision

**The bridge is a Python `rclpy` node, a client of the existing API, running
on ROS 2 Lyrical natively on Windows.**

- It publishes `/clock`, `/scan`, `/odom`, `/imu` and their transforms, and
  forwards `/cmd_vel` while it is fresh, in simulation time.
- Each cycle ends with a `step`, so the bridge runs at a steady rate of
  simulation time, in the editor or headless.
- Ground truth is published only on request, on its own topic, in a `world`
  frame, for display.
- A minimal goal driver, also in the package, demonstrates a ROS node driving
  on simulation time. It stops for obstacles rather than avoiding them.
- The API's odometry gains the velocity the encoders measured, so `/odom`
  carries a measured twist rather than one differentiated from poses, which
  would be noisy and would lag.

## Consequences

- The simulation still knows nothing about ROS. The only engine change was
  odometry velocity, which any client can use.
- The bridge holds the API's one connection, so it cannot run alongside the
  scenario runner against the same simulation.
- Headless, the simulation does not wait for the bridge, so the bridge samples
  simulation time more coarsely, about 14 cycles a simulated second at 5.7
  times real time against 30 in real time. Every lidar scan still arrives.
  Lockstep stepping would remove this, at the cost of speed.
- Native Windows ROS is unusual; most robotics teams run Ubuntu. The bridge is
  plain `rclpy` with no Windows-specific code, but has not been run on Linux.
- Pixi's activation writes Qt's plugin path in cmd.exe syntax, which
  PowerShell does not expand, so RViz cannot start without the workaround in
  `Tools/RosEnv.ps1`.
- The bridge's Python tests need no ROS, but are not in CI, which has no ROS
  installed.

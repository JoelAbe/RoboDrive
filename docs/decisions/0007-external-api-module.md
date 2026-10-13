# 0007. Expose the simulation through a TCP API in its own module

- **Status:** Accepted

## Context

A third party must be able to test the robot without knowing Unreal: load a
scenario, drive the robot, read its sensors and decide whether it passed. A
ROS 2 bridge planned for Phase 6 needs the same access. Both are clients of the
simulation, and neither should require changes to the simulation itself.

That needs a module boundary and a transport. The candidates for transport were
ZeroMQ, HTTP and raw TCP.

## Decision

A fourth module, `RoboApi`, depends on `RoboSim` and nothing depends on it. It
is loaded because the project file lists it, and starts itself.

The transport is newline-delimited JSON over TCP:

- Each request and response is one JSON object on one line. Responses are
  written condensed, because a pretty-printed object contains newlines.
- Every request carries an `id` that its response echoes, and every response
  reports `ok` with either a `result` or an `error`.
- The server binds to loopback by default and serves one client at a time,
  polled on the game thread.
- Stepping is advance-and-wait: a `step` request is answered once the clock has
  advanced by the requested number of steps.
- Sensor readings and ground truth are separate operations.

## Consequences

- Neither side needs a third-party library. `FSocket` ships with the engine and
  `socket` and `json` ship with Python, and the protocol can be debugged by
  typing at it. ZeroMQ would have meant vendored binaries under
  `Source/ThirdParty`.
- Throughput is limited. A full lidar scan as JSON text is the largest part of
  any reply, so scans can be declined or thinned. Bulk data such as camera
  images would need a separate channel rather than a different control
  protocol.
- Polling on the game thread means actors can be read and moved without any
  thread-safety concerns, at the cost of throughput this project does not need.
- With one client at a time, a second connection replaces the first, so a
  crashed client cannot lock the simulation out.
- Advance-and-wait stepping is reproducible rather than deterministic, because
  the world keeps running between requests
  ([ADR 0006](0006-physics-on-the-actor-tick.md)).
- Keeping ground truth in its own operation makes it visible which clients read
  it. Autonomy that reads ground truth works in simulation and fails on a real
  robot.
- There is no authentication, so binding beyond loopback is logged as a
  warning.
- The Phase 6 ROS bridge becomes another client of this API rather than a
  change to the simulation.

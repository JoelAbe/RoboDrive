# External API reference

The simulation exposes a TCP server so any client can drive and observe it
without knowing anything about Unreal. It exists only while a game world does:
in the editor that means while playing, or when launched headless with `-game`.

## Connection

| | |
|---|---|
| Address | `127.0.0.1` |
| Port | `8765` by default |
| Framing | One JSON object per line, terminated by a newline |
| Clients | One at a time. A new connection replaces the old one. |

The address, port and auto-start are project settings, under **Project
Settings → Plugins → RoboDrive API**, stored in `Config/DefaultEngine.ini`. The
command-line argument `-RoboApiPort=NNNN` overrides the port, which is how CI
runs on 8766 while the editor uses 8765.

**Security.** The server has no authentication and will drive the robot for
anyone who connects. It binds to loopback by default, so nothing outside the
machine can reach it, and logs a warning if configured to bind anywhere else.

## Messages

Every request is an object with an `op` naming the operation and, optionally,
an `id`:

```json
{"id": 7, "op": "set_command", "v": 0.5, "w": 0.25}
```

Every response echoes the `id` and reports success or failure:

```json
{"id": 7, "ok": true, "result": {"v": 0.5, "w": 0.25}}
{"id": 8, "ok": false, "error": "unknown op 'frobnicate'"}
```

A request without an `op` is refused. A request without an `id` is answered
with an `id` of 0. Clients should send a unique `id` with every request and
check that the reply carries it.

## Conventions

All positions, velocities and angles use ROS REP-103: metres, seconds and
radians, +X forward, **+Y to the left**, yaw positive counter-clockwise.
Times are simulation seconds, never wall-clock time.

## Operations

### `ping`

Checks the connection. Answers even if the simulation itself is unavailable.

Result: `{"pong": "RoboDrive"}`

### `get_state`

| Result field | Meaning |
|---|---|
| `sim_time` | Simulation time, seconds |
| `step` | Fixed steps taken since the world started |
| `real_time_factor` | Simulated seconds per real second; below 1 means the simulation is running slow |
| `robot` | Name of the robot actor, if there is one |
| `speed_ms` | Robot speed, m/s |
| `mission.status` | `none`, `active`, `arrived` or `failed` |
| `mission.goal` | The current goal, `{x, y, tolerance_m}`, present only when one is set |

`arrived` means whatever is driving **believes** it has reached the goal. It is
a claim, not a verdict: a test should still check `get_ground_truth`.

### `reset`

Moves the robot to a pose, stops it, and restarts its dead reckoning there.

| Request field | Required | Meaning |
|---|---|---|
| `x`, `y` | no | Target position. If omitted, the robot returns to where it first spawned. |
| `yaw` | no | Target heading. Defaults to 0. |

Result: `{"reset": true}`

**Simulation time is not rewound.** It keeps running for the life of the
world, because sensors schedule their next sample in simulation time and a
rewound clock would stall them. A client that wants elapsed time should record
`sim_time` after the reset.

### `set_command`

Submits a drive command as the `External` source.

| Request field | Required | Meaning |
|---|---|---|
| `v` | yes | Linear velocity, m/s. Positive is forward. |
| `w` | yes | Angular velocity, rad/s. Positive turns left. |

Result echoes `v` and `w`.

**Commands expire.** A command is valid for 0.25 s of simulation time. A client
must re-send its command every cycle, exactly as a ROS node republishes
`cmd_vel`, or the robot stops. This is the watchdog: a client that crashes
cannot leave the robot driving.

`External` outranks the in-engine autonomy but not manual keyboard input.
See [ADR 0005](decisions/0005-arbitration-by-priority-and-freshness.md).

### `step`

Waits until the simulation has advanced by a number of fixed 1/120 s steps,
then replies.

| Request field | Required | Meaning |
|---|---|---|
| `steps` | no | Steps to advance. Defaults to 1. |

Result: `{"sim_time": ..., "step": ...}` at the moment the target was reached.

This is **advance-and-wait**, not lockstep. The simulation keeps running
between requests; `step` guarantees that a known amount of simulation time has
passed before the client sees the result. A run is therefore reproducible to
within a few centimetres rather than bit-identical
([ADR 0006](decisions/0006-physics-on-the-actor-tick.md)). Only one step may be
outstanding at a time.

A typical control loop:

```python
while running:
    sensors = client.call("get_sensors", include_lidar=False)
    v, w = controller(sensors)
    client.call("set_command", v=v, w=w)
    client.call("step", steps=12)   # 10 Hz control
```

### `get_sensors`

What the robot could know. **Autonomy should read only this.**

| Request field | Required | Meaning |
|---|---|---|
| `include_lidar` | no | Include the lidar scan. Defaults to true. |
| `lidar_stride` | no | Return every Nth beam. Defaults to 1. |

| Result field | Meaning |
|---|---|
| `sim_time` | Simulation time of the reply |
| `odometry` | Pose estimated from wheel encoders: `{x, y, yaw}` |
| `travelled_m` | Distance the encoders believe has been travelled |
| `imu.accel` | Specific force `[x, y, z]`, m/s². Reads about +9.81 on z at rest. |
| `imu.gyro` | Angular velocity `[x, y, z]`, rad/s |
| `imu.time` | Simulation time of the IMU sample |
| `lidar.angle_min` | Angle of the first beam, rad |
| `lidar.angle_increment` | Angle between returned beams, rad. Scaled by the stride. |
| `lidar.range_min`, `lidar.range_max` | Sensor range limits, m |
| `lidar.time` | Simulation time of the scan |
| `lidar.ranges` | Range per beam, m, or `null` for no return |

A `null` range means the beam saw nothing: no target, a dropout, or a reading
outside the sensor's limits. It is deliberately distinct from a reading at
maximum range. Treating "saw nothing" as "nothing is there" is how a robot
drives into glass.

### `get_ground_truth`

What the simulation knows and the robot does not. **This is the oracle a test
judges against; autonomy must never read it**, or it will work in simulation
and fail on a real robot.

| Result field | Meaning |
|---|---|
| `sim_time` | Simulation time of the reply |
| `pose` | True pose: `{x, y, yaw}` |
| `odometry_error_m` | Distance between the odometry estimate and the truth |
| `heading_error_rad` | Odometry heading minus true heading |
| `speed_ms` | True speed, m/s |
| `velocity` | True velocity `[x, y, z]`, m/s |

### `load_scenario`

Builds the world a scenario describes: removes the previous scenario's
obstacles, places new ones, moves the robot to the start pose and marks the
goal.

| Request field | Required | Meaning |
|---|---|---|
| `scenario` | yes | The scenario's contents as an object, not a path to a file |

Result: `{"name": ..., "obstacles": <count>}`

The contents are sent rather than a path so the simulation never needs access
to the client's filesystem. The whole scenario is validated before anything
changes; a malformed one is refused and the previous world is left standing.
The format is described in [scenarios.md](scenarios.md).

If the scenario has a goal, it becomes the mission goal and the mission becomes
`active`. If it has none, any previous goal is cleared, so a scenario never
inherits another's goal.

### `set_goal`

Gives the robot somewhere to go. The mission becomes `active`.

| Request field | Required | Meaning |
|---|---|---|
| `x`, `y` | yes | Goal position |
| `tolerance_m` | no | Radius within which the robot counts as arrived. Defaults to 0.5; must be positive. |

Result: `{"status": "active"}`

The goal belongs to the mission rather than to any particular brain, so the
in-engine autonomy and an external client can execute the same goal on equal
terms.

### `clear_goal`

Withdraws the goal. The mission returns to `none`.

Result: `{"status": "none"}`

## Errors

Errors are reported as `"ok": false` with an `error` string. The connection
stays open.

| Error | Cause |
|---|---|
| `malformed JSON` | The line was not a JSON object |
| `missing 'op'` | The request had no operation name |
| `unknown op '...'` | No such operation |
| `simulation subsystem unavailable` | The world is not running a simulation |
| `mission subsystem unavailable` | The world has no mission subsystem |
| `no robot in the world` | The operation needs a robot and there is none |
| `a step is already in progress` | A second `step` arrived before the first completed |
| `set_command needs both v and w` | A drive command was missing a velocity |
| `set_goal needs x and y` | A goal was missing a coordinate |
| `tolerance_m must be positive` | A goal tolerance of zero or less, which could never be reached |
| `load_scenario needs a 'scenario' object` | No scenario was sent |
| `missing or unsupported schema_version (expected 1)` | The scenario is for a different format version |
| `scenario has no 'start'` | The scenario has no start pose |
| `obstacle N needs x, y and a three-element size` | An obstacle is malformed |

## Python client

`Tools/Analysis/robodrive_client.py` wraps the protocol. It allocates ids,
checks they match, raises `ApiError` when a request fails, and closes the
connection however the block ends:

```python
from robodrive_client import SimClient, ApiError

with SimClient(port=8765) as client:
    state = client.call("get_state")
```

Its defaults can be set with the `ROBODRIVE_HOST` and `ROBODRIVE_PORT`
environment variables.

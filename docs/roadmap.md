# Roadmap

Each phase is independently shippable. Validation runs through every phase: a
phase is not done until its tests and documentation exist.

| Phase | Scope | Status |
|---|---|---|
| 0 | Foundations: module structure, coordinate frames, test runner | Done |
| 1 | Vehicle, differential-drive kinematics, control arbitration | Not started |
| 2 | Sensors, odometry, deterministic time, record and replay | Not started |
| 3 | External simulation API and data-driven scenarios | Not started |
| 4 | In-engine autonomy, judged by the scenario suite | Not started |
| 5 | Operator view | Not started |
| 6 | ROS 2 adapter (software-in-the-loop) | Not started |

## Phase 0: Foundations

Three-module C++ structure with an enforced dependency direction, Unreal to
REP-103 coordinate conversions with tests, and a script that runs every test
without opening the editor.

**Done when:** one command runs the full test suite headless.

## Phase 1: It drives

A `(linear velocity, angular velocity)` drive command, differential-drive
kinematics, a raycast-wheel vehicle on Chaos physics, and a control arbiter
that selects between command sources. Manual input always overrides, and a
watchdog stops the vehicle if the active source goes quiet.

**Done when:** the robot is driven by keyboard, can be switched to a stub
autonomy and back, and stops by itself if that stub is killed mid-drive.

## Phase 2: It senses, deterministically

Sim-time clock with real-time and synchronous stepping modes. Wheel encoders,
IMU, lidar and camera, each with its own update rate and noise model. Ground
truth published separately from sensor output. Wheel odometry with estimated
and true pose visualised, record and replay, a profiling pass on sensor cost,
and Python analysis of recorded drift.

**Done when:** drift is visible in-engine and quantified in a plot, and a
recorded run replays identically.

## Phase 3: External API and scenarios

A transport-neutral simulation API over a socket, a Python client, and
scenarios defined as data: layout, goal, sensor configuration, weather, and
pass/fail criteria. Scenario regression tests are written as Python clients.

**Done when:** a Python script loads a scenario, steps it, drives the robot and
returns pass or fail, with no Unreal knowledge required.

## Phase 4: In-engine autonomy

Goal seeking with lidar-based obstacle avoidance, evaluated by the Phase 3
scenario suite.

**Done when:** the autonomy passes a set of scenarios, including a documented
failure case in fog.

## Phase 5: Operator view

Telemetry panel, mode switching, runtime-tunable parameters.

**Done when:** someone unfamiliar with the project can change modes and
parameters and understand what the robot is doing.

## Phase 6: ROS 2 adapter

A bridge running in Docker that is just another client of the Phase 3 API,
publishing sensor topics and `/clock`, and subscribing to `/cmd_vel`.

**Done when:** a ROS 2 node drives the robot in sim time, visualised in RViz.

## Out of scope

Hardware-in-the-loop, real-world experiments, detailed art assets, and
distributed or multi-machine simulation.

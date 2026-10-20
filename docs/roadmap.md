# Roadmap

Each phase is independently shippable. Validation runs through every phase: a
phase is not done until its tests and documentation exist.

| Phase | Scope | Status |
|---|---|---|
| 0 | Foundations: module structure, coordinate frames, test runner | Done |
| 1 | Vehicle, differential-drive kinematics, control arbitration | Done |
| 2 | Sensors, odometry, deterministic time, record and replay | Done |
| 3 | External simulation API and data-driven scenarios | Done |
| 4 | In-engine autonomy, judged by the scenario suite | Done |
| 5 | Operator view | In progress |
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

Profiling results for the lidar are recorded in
[profiling-lidar.md](profiling-lidar.md).

Sim-time clock with real-time and synchronous stepping modes. Wheel encoders,
IMU, lidar and camera, each with its own update rate and noise model. Ground
truth published separately from sensor output. Wheel odometry with estimated
and true pose visualised, record and replay, a profiling pass on sensor cost,
and Python analysis of recorded drift.

**Done when:** drift is visible in-engine and quantified in a plot, and a
recorded run can be replayed.

Replay was planned to be identical. It was measured instead: a replayed run
diverged by at most 0.033 m over 25 m travelled, which is well inside any
scenario tolerance, so bit-identical replay was not pursued
([ADR 0006](decisions/0006-physics-on-the-actor-tick.md)).

## Phase 3: External API and scenarios

A simulation API over a TCP socket with newline-delimited JSON, a Python
client, and scenarios defined as data: start pose, obstacles, goal and time
limit. Scenario regression tests are written as Python clients and run in CI
on a self-hosted runner.

Weather and per-scenario sensor configuration were planned here and moved to
Phase 4, where the fog failure case is the only thing that needs them.

**Done when:** a Python script loads a scenario, steps it, drives the robot and
returns pass or fail, with no Unreal knowledge required.

## Phase 4: In-engine autonomy

Goal seeking with lidar-based obstacle avoidance, evaluated by the Phase 3
scenario suite. Scenarios gain a visibility setting that degrades the lidar,
so autonomy can be shown failing for a documented, physical reason.

**Done when:** the autonomy passes a set of scenarios, including a documented
failure case in fog.

Goal seeking and reactive avoidance are done
([ADR 0010](decisions/0010-reactive-obstacle-avoidance.md)). The slalom,
written in Phase 3 as a known failure, now passes and guards avoidance against
regressions.

Fog is modelled as range loss and clutter on the lidar
([ADR 0011](decisions/0011-fog-as-lidar-attenuation-and-clutter.md)). The
documented failure is in [fog.md](fog.md): clutter, not lost range, defeats the
avoidance, paralysing the robot in dense fog and, in light fog, corrupting its
odometry until it claims to have arrived short of the goal.

## Phase 5: Operator view

Telemetry panel, mode switching, runtime-tunable parameters.

**Done when:** someone unfamiliar with the project can change modes and
parameters and understand what the robot is doing.

The [operator view](operator-view.md) shows telemetry, the brain's current
activity and ground truth set apart, with Auto and Manual modes and an
emergency stop ([ADR 0013](decisions/0013-operator-modes-and-emergency-stop.md),
[ADR 0014](decisions/0014-operator-view-in-slate.md)). The brain, avoidance and
weather can be tuned from the panel, and scenarios chosen, restarted and
unloaded there ([ADR 0015](decisions/0015-scenario-loading-in-the-simulation.md)).
A top-down lidar view shows what the brain sees, and goals can be set by
clicking the floor. Still to come: the test the phase is defined by, with
someone who has not seen the project.

## Phase 6: ROS 2 adapter

A bridge running in Docker that is just another client of the Phase 3 API,
publishing sensor topics and `/clock`, and subscribing to `/cmd_vel`.

**Done when:** a ROS 2 node drives the robot in sim time, visualised in RViz.

## Out of scope

Hardware-in-the-loop, real-world experiments, detailed art assets, and
distributed or multi-machine simulation.

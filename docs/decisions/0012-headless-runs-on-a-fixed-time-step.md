# 0012. Run headless simulations on a fixed time step

- **Status:** Accepted

## Context

Headless runs, in CI and for measurements, were paced by the wall clock, like
play in the editor. The scenario suite took 100 s for 100 s of simulation,
and the fog sweep about half an hour, most of it spent waiting. Nothing about
a headless run needs real time: nobody is watching, and the simulation keeps
its own clock.

There are two ways to run faster than real time:

- **Time dilation**, which makes each frame cover more simulated time. The
  steps get longer, so the spring-damper suspension and the tyre friction,
  which are computed per frame, behave differently and can go unstable. A
  faster run would then be testing a different robot.
- **A fixed time step**, which keeps each frame at exactly one simulation step
  but stops the engine waiting for the wall clock between frames.

## Decision

`Tools/RunScenarios.ps1` launches the headless simulation with
`-UseFixedTimeStep -FPS=120`. Each frame advances exactly 1/120 s, which
matches both the simulation clock's step and the physics substep, so every
frame is one of each. `-RealTime` restores wall-clock pacing. Play in the
editor is unchanged.

The real-time factor is measured against the wall clock rather than the
frame's DeltaTime, because in fixed-time-step mode DeltaTime is always exactly
one step, whatever real time the frame took. The scenario harness also reports
its own speed, measured from the client's side.

## Consequences

- Measured: the suite ran 100 s of simulation in 8 s, 12.3 to 12.6 times real
  time. The 39-run fog sweep took about 2 minutes instead of about 30.
- Results match real-time runs: the same verdicts, the same times to a tenth of
  a second, and the same fog sweep pattern.
- Runs are still not bit-identical. Repeated runs differ by a few centimetres,
  as in [ADR 0006](0006-physics-on-the-actor-tick.md). Fixed frame times remove
  one source of variation, but the client is not in lockstep: when a scenario
  is loaded, relative to the lidar's sweep and the brain's decisions, depends
  on when the client's request arrives. That is the likely remaining source.
- The default runs, where the robot's own brain drives, are unaffected by
  speed, since nothing in the simulation reads the wall clock. An external
  client that drives the robot is a separate program, and its own pauses are
  multiplied by the speed-up: a 15 ms stall at 12 times real time is 0.18 s of
  simulation, close to the 0.25 s command watchdog. A lockstep API, which the
  clock's synchronous mode could support, would remove this.
- The speed is limited by the cost of a frame. It is high here because the
  headless simulation renders nothing. The camera produces no images under
  `-nullrhi`, so a scenario that needs camera images cannot use this.
- CI uses the same script, so it got faster without changing the workflow.

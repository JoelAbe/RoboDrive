# Fog: a documented failure

The robot steers around obstacles using its lidar. These runs measure what
fog does to that, using the simulation's ground truth as the reference.

The short version: fog defeats the robot through **clutter**, not through lost
range. Below about 20 m visibility the robot is paralysed. Above it, the robot
still moves, but its steering becomes jittery, its wheel odometry drifts, and it
announces it has arrived when it has not. In light haze, 2 runs in 3 fail that
way.

## The fog model

Fog is set per scenario as meteorological visibility, `visibility_m`, and
affects the lidar in two ways
([ADR 0011](decisions/0011-fog-as-lidar-attenuation-and-clutter.md)):

- **Range loss.** Fog scatters light on the way out and back, so returns from
  distant surfaces become too weak to detect. The 12 m lidar sees 8.6 m at
  100 m visibility and 3.3 m at 10 m.
- **Clutter.** Some scattered light comes straight back, so beams return from
  the fog itself, mostly at close range. How readily this happens is a property
  of the sensor, the lidar's fog echo fraction, set to 0.1 for these runs.

The brain receives clutter as ordinary ranges. A real sensor reporting only its
first echo cannot tell them apart either.

## Method

`Tools/Analysis/fog_sweep.py` runs a scenario at a range of visibilities, each
twice over: once with the lidar's fog echoes on, so both effects act, and once
with them off, which leaves only the range loss. Fog echoes are random, so each
case is run three times. The robot's in-engine avoiding brain drives, and the
harness judges the result against ground truth.

The scenario is the slalom: three posts on the line to a goal 10 m ahead,
60 s allowed. A second sweep removes the posts and keeps everything else.

## In one comparison

The slalom at 10 m visibility, the same start, one setting changed. With fog
echoes on, the robot is boxed in by echoes close around it and barely moves.
With them off, leaving only the shorter range, it drives through.

<!-- VIDEO: on github.com, edit this file and drag docs/images/github/slalom-fog-default-1280.mp4 onto the next line. -->

<!-- VIDEO: on github.com, edit this file and drag docs/images/github/slalom-fog-echoless-1280.mp4 onto the next line. -->

## Results

### The slalom

| Visibility | Lidar sees to | Fog echoes on | Fog echoes off |
|---|---|---|---|
| clear | 12.0 m | 3/3, 21.5 s | |
| 100 m | 8.6 m | 1/3, 25.6 s | 3/3, 21.6 s |
| 50 m | 7.0 m | 1/3, 37.1 s | 3/3, 21.5 s |
| 30 m | 5.7 m | 1/3, 48.7 s | 3/3, 21.5 s |
| 20 m | 4.7 m | 0/3 | 3/3, 21.5 s |
| 15 m | 4.1 m | 0/3 | 3/3, 21.7 s |
| 10 m | 3.3 m | 0/3 | 3/3, 21.7 s |

Cells give the runs that reached the goal, and their mean time. How the other
runs failed, with fog echoes on:

| Visibility | Believed it arrived, but was | Timed out |
|---|---|---|
| 100 m | 0.81 m, 1.69 m from goal | |
| 50 m | 1.93 m, 0.52 m | |
| 30 m | 1.61 m, 0.83 m | |
| 20 m | 0.73 m, 0.58 m | 1 run, 2.5 m from goal |
| 15 m | | 3 runs, 2.9 to 4.9 m from goal |
| 10 m | | 3 runs, 7.7 m from goal |

### The same course without the posts

| Visibility | Fog echoes on | Fog echoes off |
|---|---|---|
| clear | 3/3, 20.9 s | |
| 100 m | 1/3, 23.1 s | 3/3, 20.9 s |
| 50 m | 2/3, 27.6 s | 3/3, 20.9 s |
| 30 m | 1/3, 32.2 s | 3/3, 20.9 s |

Every failure here was a false arrival, 0.72 to 1.76 m from the goal.

### The slalom again, headless

The slalom sweep was repeated headless, faster than real time
([ADR 0012](decisions/0012-headless-runs-on-a-fixed-time-step.md)).

| Visibility | Fog echoes on | Fog echoes off |
|---|---|---|
| clear | 3/3, 21.5 s | |
| 100 m | 1/3, 25.7 s | 3/3, 21.5 s |
| 50 m | 0/3 | 3/3, 21.6 s |
| 30 m | 0/3 | 3/3, 21.6 s |
| 20 m | 0/3 | 3/3, 21.5 s |
| 15 m | 0/3 | 3/3, 21.6 s |
| 10 m | 0/3 | 3/3, 21.6 s |

The same pattern: every run without echoes succeeds, every failure at 30 m and
above is a false arrival, and below 20 m the robot runs out of time. Across both
slalom sweeps, runs with echoes reached the goal 2 times in 6 at 100 m, once in
6 at 50 m and 30 m, and never at 20 m or below.

### Odometry error at the end of each run

| | Fog echoes off | Fog echoes on |
|---|---|---|
| Slalom | 0.01 to 0.05 m, and 0.24 and 0.36 m in two runs at 15 and 10 m | 0.17 to 1.78 m |
| Without posts | 0.01 to 0.02 m | 0.05 to 1.77 m |

## What the runs show

**Range loss alone is harmless here.** With fog echoes off, every run reached
the goal in the same time as clear air, down to 10 m visibility where the lidar
sees only 3.3 m. The avoidance looks 1.5 m ahead at 0.5 m/s, which is 3 s of
travel, so seeing 3.3 m is plenty. A faster robot or a longer lookahead would
change that.

**Below about 20 m, clutter paralyses the robot.** Fog echoes close to the
sensor fall inside the corridor of almost every heading, so most scans offer no
clear way forward. The robot stops, turns, occasionally lurches a short way,
and runs out of time. At 10 m it covered about 2.3 m in 60 s.

It never reported giving up. The brain gives up after 5 s of being
continuously boxed in, and clutter is random from scan to scan, so now and
then one scan shows a clear heading and resets the timer. A rule written for
an obstacle that stays put does not fire for clutter that flickers. Measuring
progress toward the goal would catch both.

**Above it, the robot is fooled about where it is.** At 30 m to 100 m the robot
still moves, but two thirds of runs end with the brain announcing arrival while
up to 1.9 m short, and with odometry errors 10 to 100 times those of the same
runs without echoes. Fog never touches the wheels, so it must be reaching
odometry some other way.

The mechanism is the steering. Clutter makes the chosen heading jump from scan
to scan, so the commands lurch every 0.1 s between driving, stopping and
turning either way. The body cannot follow changes that abrupt and the tyres
slip. The simulated wheel encoders count the wheel rotation that was commanded,
so slip is counted as progress that never happened. Hundreds of small lurches
add up to a metre or more of false progress, and the brain, which navigates by
odometry, stops at a goal it has not reached.

**It is not contact with the posts.** The harness does not detect collisions,
so scraping a post while zigzagging could have produced the same symptom.
Without the posts, 5 of 9 runs still failed the same way, with the same size of
odometry error. The two highest errors with echoes off, at 15 m and 10 m, fit
the same explanation on a smaller scale: there the posts are seen late and the
swerves are sharper.

This is a cascade: a degraded lidar corrupts an estimate that never uses the
lidar, and the link between them is the controller.

## Caveats

- **The size of the odometry effect is probably overstated.** In this model a
  wheel reaches its commanded speed instantly, as if its motor had unlimited
  torque, so every shortfall becomes tyre slip, and the encoder reads commanded
  rather than actual rotation. On a robot, motors take time to spin up and
  encoders measure the wheel's real rotation. The mechanism is real; its
  magnitude here is likely larger than on hardware.
- **The fog echo fraction is assumed, not calibrated.** Real lidars differ
  widely in how readily they echo off fog. A different value moves the cliff;
  it does not remove the failure.
- **Few runs per case.** Three per sweep, six across both slalom sweeps. The
  pass rates are rough; the pattern across visibilities and between echoes on
  and off is what the evidence supports, and it repeated.
- **Fog echoes are independent from beam to beam and scan to scan.** Real fog
  has structure, such as denser patches, which this model does not have.

## What would fix it

None of these is implemented. In rough order of how directly each addresses
the failure:

- **Filter clutter before avoiding it.** Fog echoes flicker; posts persist.
  Requiring a return to appear in consecutive scans, or in a cluster of
  neighbouring beams, before treating it as an obstacle would remove most fog
  echoes and keep every real obstacle.
- **Use the last echo.** Many lidars can report several echoes per pulse. The
  last one usually comes from the solid surface behind the fog.
- **Smooth the commands.** An acceleration limit between the brain and the
  wheels would stop jittery decisions becoming slip.
- **Navigate on more than wheel odometry.** The gyro's heading stayed within
  about a degree in [the drift measurements](odometry-drift.md) while odometry
  drifted by up to 25°. Fusing the two, as `robot_localization` does in ROS,
  limits what slip can do to the estimate.
- **Give up on lack of progress, not on continuous blocking,** so a flickering
  blockage is reported as a failure instead of running out the clock.

## The scenario

`Scenarios/slalom_fog.json` is the slalom at 10 m visibility, marked as a known
failure. It runs in CI with the rest of the suite. When a fix makes it pass,
the harness reports `XPASS` and fails the run until the marker is removed.

## Reproducing

Headless, with the editor closed, in about two minutes:

```
./Tools/RunScenarios.ps1 -Script Tools\Analysis\fog_sweep.py
./Tools/RunScenarios.ps1 -Script Tools\Analysis\fog_sweep.py -ScriptArgs "--without-obstacles --visibilities 100 50 30"
```

Or against the editor while playing, in real time, which for the first takes
about half an hour:

```
python Tools/Analysis/fog_sweep.py
```

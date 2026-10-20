# The operator view

While the simulation is playing, panels over the game view show what the robot
is doing and let you take control of it. They cover the edges of the screen and
leave the middle clear for the world. **Tab** hides and shows them.

## Keys

| Key | Does |
|---|---|
| W, A, S, D | Drive the robot. Overrides whatever else is driving while held. |
| **1** | Auto mode: the robot's brain drives, and you can take over by driving. |
| **2** | Manual mode: only you drive. Let go and the robot stops. |
| **Space** | Emergency stop. Releasing it takes the button in the top bar. |
| **R** | Restart the current scenario. |
| Left click on the floor | Give the robot a goal there. |
| Right click | Withdraw the goal. |
| **Tab** | Hide or show the panels. |

## The top bar

The state of the simulation: whether it is running, which scenario is loaded,
the simulated time, and how fast it runs against the wall clock. Below 1x, the simulation cannot keep
up.

On the right are the mode buttons and the emergency stop. The selected mode is
filled. While the stop is engaged the bar turns coral and the button reads
**Release stop**.

## The robot panel

From top to bottom:

- **Driving**: which command source the arbiter picked, or Stopped.
- **Command and actual**: the velocity asked for, above the velocity the robot
  is actually moving at. The gap is the vehicle's own response: acceleration,
  grip and slip.
- **Brain**: which brain is selected, and in a sentence what it is doing now:
  heading for the goal, steering around something and by how much, or boxed in
  and for how long. In Manual mode or while stopped it says the brain is being
  ignored instead.
- **Mission**: the goal, and how far away the robot believes it is.
- **Lidar and weather**: the sensor, and the fog it is working in.
- **Ground truth**: how far odometry has drifted from where the robot really is.
  The simulation knows this and the robot does not, so it is boxed and labelled
  as never shown to the robot. **Reset odometry to the truth** sets the robot's
  belief back to where it really is: the operator correcting the robot, as a
  pose estimate does in ROS. The brain is given a corrected belief; it still
  never reads the truth itself. The drawn paths start again from there.

## The lidar view

At the foot of the left column, a top-down drawing of what the robot sees, in
its own frame, facing up: the lidar returns, the direction it believes the goal
is in, the heading it chose, and the corridor it checked along that heading.
Range rings mark each metre, out to 5 m.

It is drawn from exactly what the brain receives. Fog echoes therefore look
like any other return, because to the brain they are, and the goal direction
comes from odometry, so it is wrong when odometry is.

## Goals

A left click on the floor gives the robot a goal there; a right click withdraws
it. Clicks on the panels never reach the world. The goal is drawn as a ring on
the floor, amber while the robot is on its way, teal once it believes it has
arrived and coral if it gave up, whether it came from a click, a scenario or
the API.

## The scenario panel

At the top of the right-hand column. **Choose** lists every file in
`Scenarios/`, read afresh each time it opens, plus **None**. Picking a file
builds its world; picking None removes the current scenario and returns the
robot to where it spawned. If a file cannot be loaded, the reason is shown
beneath the list.

**Restart**, or R, starts the current scenario from the beginning, restoring
its goal and weather even if they were changed since. Simulation time keeps
running rather than rewinding, because sensors have already scheduled their
next samples against it.

## The tuning panel

Below the scenario panel. Every change applies at once.

- **Brain**: Go to goal, or Avoiding.
- **Max speed**, **avoidance lookahead** and **avoidance margin**. Changing a
  brain setting rebuilds the brain, so it forgets what it remembered: whether
  it had arrived, and how long it had been boxed in.
- **Weather**: Clear or Fog. In fog, the **visibility** slider runs from 5 m to
  200 m on a logarithmic scale, because visibility matters across orders of
  magnitude. **Lidar fog echoes** sets how readily the lidar mistakes fog for a
  target; at zero, fog only shortens its range. Setting it to zero in thick fog
  shows the finding in [fog.md](fog.md): the robot recovers.
- **Show in the world**: lidar rays, the odometry and true paths, the brain's
  chosen heading, and the floating text readouts, which start hidden because
  the panels show the same information.

The panel reads the settings back every frame, so a change made in the
editor's Details panel shows here too.

## Colours

| Colour | Means |
|---|---|
| Teal | Running normally: autonomy driving, the goal reached |
| Amber | Worth watching: you driving, a mission under way, fog, steering around |
| Coral | Stopped or failed |
| Violet | An external client, such as a test script, is driving |

## How it is built

The view is written in C++ with Slate rather than as a Widget Blueprint, so the
layout can be reviewed like any other code
([ADR 0014](decisions/0014-operator-view-in-slate.md)). Modes and the
emergency stop are rules in the arbiter
([ADR 0013](decisions/0013-operator-modes-and-emergency-stop.md)).

The view is not created in headless runs, where nothing is rendered.

# 0004. Model the vehicle with raycast wheels and casters

- **Status:** Accepted

## Context

The robot needs a physical model where the differential drive kinematics are
visible rather than buried. Three options were considered:

- **Chaos Vehicles plugin.** Provides tyre and suspension models, but its
  abstractions are built around cars with engines, transmissions and
  differentials, and the kinematics of interest would sit inside the plugin.
- **Constraint-driven wheels.** Each wheel a rigid body on a physics constraint
  with an angular motor. The most physically faithful option, and the most
  time spent tuning constraints rather than building robotics.
- **Raycast wheels.** A trace per wheel provides suspension; drive and lateral
  forces are applied at the contact patch.

## Decision

Raycast wheels on a single Chaos rigid body chassis.

Each wheel traces along its suspension axis. A spring and damper force, clamped
so it can only push, holds the chassis up. Driven wheels additionally apply the
force needed to correct their longitudinal speed error and to cancel sideways
motion, with both components limited by a shared friction circle scaled by the
suspension load.

The layout is a true differential drive: two driven wheels on a central axle,
with a free-rolling caster at each end.

## Consequences

- Non-holonomic behaviour is not asserted anywhere in code. It emerges from
  wheels resisting sideways motion, and casters not resisting it.
- Wheel slip is modelled: a wheel demanding more force than its share of grip
  allows is scaled back, which is what a skidding wheel does. This is what
  Phase 2 wheel odometry will drift against.
- Every part of the model is a readable function, and the parts that are pure
  maths are unit tested without a world.
- There is no tyre model in the automotive sense: no slip curves, no camber, no
  load sensitivity beyond the friction circle.
- Wheels do not physically rotate. Rotation is derived from contact patch speed,
  which is what the Phase 2 encoders read.
- Driving all four wheels and giving the casters lateral grip would make this a
  skid-steer. That is a deliberate future variant for comparing odometry
  quality between steering geometries, not the default.
- Forces are currently applied on the actor tick while physics substeps at
  120 Hz, so a force sized to correct an error in one step is applied across
  several. Phase 2 moves this onto the physics tick.

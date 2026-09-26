# 0005. Arbitrate drive commands by priority and freshness

- **Status:** Accepted

## Context

Several sources drive the same robot: keyboard input, an in-engine autonomy, an
external test client, and eventually a ROS 2 stack. Something has to decide
which one is in control at any moment.

Two situations make a simple mode flag inadequate:

- A human must be able to take over immediately, by touching the controls,
  rather than by first switching a mode.
- A source that stops sending must not leave the robot executing its last
  order. An autonomy that crashes while commanding 1.5 m/s would otherwise
  drive until something stopped it physically.

## Decision

Every command carries a timestamp and the identity of its source. Sources have
a fixed priority: Manual, then External, then Autonomy. An arbiter selects the
highest-priority source whose most recent command has not expired, where
expiry is measured from the command's own timestamp with a default window of
0.25 s. If no command is fresh, the output is a zeroed command, which is an
active instruction to stop rather than an absence of instruction.

The current time is passed into the arbiter rather than read from the engine.

## Consequences

- Manual override and a watchdog fall out of one rule rather than being two
  mechanisms. Releasing the controls lets manual input expire and hands control
  back without an explicit release path.
- Expiry measured from the issuing timestamp means a command delayed in transit
  from an external client is correctly treated as stale.
- Manual input must be submitted only while the operator is actually driving.
  Submitting zeros continuously would keep manual permanently in control. On a
  real vehicle this gating is a deadman switch.
- Priority is encoded as the numeric order of the source enum, so reordering it
  would silently change behaviour. A test pins the order.
- Passing time in makes the arbiter testable without a world and keeps it
  correct under the simulated clock introduced in Phase 2.
- Arbitration is per-source, not per-command: a source cannot queue commands,
  and its latest command always replaces its previous one.

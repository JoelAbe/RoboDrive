# 0013. Make modes a gate and the emergency stop a latch

- **Status:** Accepted

## Context

An operator needs two controls over who drives the robot: a way to take sole
control, and a way to stop it outright. Both have to coexist with the arbiter
([ADR 0005](0005-arbitration-by-priority-and-freshness.md)), which picks the
highest-priority source with a fresh command and stops the robot when none is
fresh.

The obvious designs reuse the arbiter's priorities:

- **Manual mode as a priority.** But manual input already outranks everything.
  The point of a manual mode is what happens when the operator lets go of the
  keys: the robot should stop, not be handed back to the brain.
- **The emergency stop as a top-priority source.** But every source's command
  expires after 0.25 s. That watchdog is what makes a crashed client harmless,
  and it would also release a stop a quarter of a second after it was pressed.

## Decision

Both live in the arbiter, outside the priority order.

- **Modes are a gate.** In Auto, any source may drive and priority decides. In
  Manual, the arbiter ignores autonomy and external commands entirely, so only
  a person at the keys can drive.
- **The emergency stop is a latch.** While engaged, the arbiter returns a stop
  whatever any source sends, a person at the keys included. It stays engaged
  until it is explicitly released.
- Resetting the arbiter clears submitted commands but keeps the mode and the
  stop, which are the operator's decisions, not the scenario's.

In the operator view, Space engages the stop from anywhere. Releasing it takes
the button in the panel.

## Consequences

- A stop cannot expire. A unit test checks it is still in force long after it
  was engaged, so it cannot quietly be turned back into a source later.
- Letting go of the keys in Manual mode stops the robot, as an operator taking
  control would expect.
- The stop can be engaged without looking but released only deliberately, as
  with a physical emergency stop that is pressed to stop and twisted to reset.
- Brains keep deciding while ignored, so they resume immediately when control
  returns. The operator view says when a brain is being ignored, so its plans
  are not mistaken for what the robot is doing.
- The arbiter stays plain C++ with the current time passed in, so both rules
  are tested without a running world.
- The external API cannot yet engage or release the stop, or change the mode.
  A ROS bridge would want both, as an emergency stop topic.

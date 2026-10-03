# 0006. Keep physics forces on the actor tick

- **Status:** Accepted

## Context

Wheel forces are computed and applied on the actor tick, using the frame
delta, while Chaos substeps physics at a fixed 120 Hz. A force sized to correct
a velocity error "in one step" is therefore applied across several substeps,
and the work is driven by a rate that varies with rendering.

The alternative is Unreal's async physics tick, which runs at a fixed rate on
the physics thread. It is the technically correct home for this work, and it
also carries real cost: the callback runs outside the game thread, with rules
about what may be touched, and it would have to be threaded through every
sensor that samples vehicle state.

The question is whether the current arrangement is reproducible enough to
support replaying a recorded run as a regression test, which is what Phase 3
needs it for.

## Decision

Keep force application on the actor tick for now.

Replaying a recorded command stream was measured against the original run: the
robot diverged by at most **0.033 m over 25 m travelled**, about 0.13% of
distance. That is well inside any tolerance a scenario test would use, so the
move to the physics tick is not required to make replay useful.

A long frame remains a special case: frames longer than 0.1 s are skipped
rather than applying forces sized for a step the physics budget cannot cover.

## Consequences

- Scenario tests in Phase 3 can assert outcomes with centimetre-scale
  tolerances rather than requiring bit-identical reproduction.
- Replay is reproducible, not deterministic. Two replays of the same file will
  agree closely but not exactly, so a test must never compare for equality.
- Behaviour still depends on frame rate at the margins. A machine much slower
  or faster than this one may show different divergence, and the figure above
  should be re-measured rather than assumed to transfer.
- Moving to the async physics tick stays available, and this measurement is the
  baseline any future change would have to beat.

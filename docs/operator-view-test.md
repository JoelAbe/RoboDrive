# Operator view: the newcomer test

Phase 5 is done when *someone unfamiliar with the project can change modes and
parameters and understand what the robot is doing*. That cannot be judged by
the person who built it, so it was tested with someone who had never seen the
project.

## Method

The participant sat at the running simulation with no scenario loaded and was
told only that it was a simulated robot, and to think aloud. They were then
given these tasks one at a time, read out as written, with no help unless they
were stuck for a full minute:

1. Make the robot drive to the far wall.
2. Stop it, right now. Then get it going again.
3. Drive it yourself somewhere, then let it drive itself again.
4. Load the slalom. Before it reaches the first post, say which way it will go
   round it, and why.
5. Make it go faster.
6. Is the robot where it thinks it is?
7. Make the robot fail, then say why it failed.

Tasks 4, 6 and 7 test *understanding* rather than operation: they need the
lidar view, the ground-truth box and the fog settings, without being pointed
at any of them.

## Result

Every task was completed without help, quickly.

The one hesitation was the first task. It took about a minute to discover that
clicking the floor gives the robot a goal. Once found, it was used without
further trouble.

## What changed

The hint under the mission, *Click the floor for a goal, right-click to
clear*, had been drawn in the faintest text colour from the start. It is now
drawn at full strength while the robot has no goal, which is exactly when a
newcomer needs it, and fades back once a goal has been set.

## Limitations

This was one participant, observed informally by the developer, so it shows
that the view can be understood unaided rather than measuring how well. Timings
were not recorded beyond the one hesitation. A firmer test would use several
participants, record time per task, and include someone from a robotics
background alongside someone without one.

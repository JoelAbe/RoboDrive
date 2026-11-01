# 0016. Filter lidar clutter by persistence, and smooth the brain's commands

- **Status:** Accepted

## Context

Fog defeated the obstacle avoidance ([fog.md](../fog.md)). The avoiding brain
treated every lidar return as an obstacle, so fog echoes close to the sensor
blocked its corridors. In dense fog the robot was paralysed. In lighter fog
the chosen heading jumped from scan to scan, the commands lurched, the wheels
slipped, and odometry, which counts commanded wheel rotation, recorded
progress that never happened, until the brain announced arrival short of the
goal.

Two things were wrong: what the brain believed was an obstacle, and how
abruptly it acted on its decisions.

The options for the first included range gating (ignoring close returns,
which would also ignore close obstacles), a minimum cluster size (fog echoes
are scattered, but a thin pole can be a single return), multi-echo or
last-return sensing (a sensor change, not an autonomy one), and persistence:
requiring a return to be seen repeatedly. Persistence was modelled first in a
quick 2D simulation, which suggested three scans at a 0.15 m radius.

## Decision

**A return counts as an obstacle only if one was seen within 0.15 m of it in
each of the previous two scans**: three in a row. A real obstacle stays put; a
fog echo lands somewhere new every scan.

- Scans are compared in the **odometry frame**, so a fixed object lines up
  however the robot moved between them (ego-motion compensation). Only the
  drift between scans a tenth of a second apart matters, which is millimetres.
- Each scan is stored once, by its timestamp, so a scan the brain happens to
  run on twice cannot confirm itself.
- Until three scans have been seen, the brain holds still, as it does with no
  scan at all.
- The filter is a setting, `PersistenceScans`, where 1 switches it off, and
  the operator view can switch it off to show the failure it fixes.

**The brain's commands are smoothed** on their way to the wheels, limited to
1 m/s² and 2 rad/s², as the keyboard's commands already were.

## Consequences

- At 10 m visibility the slalom went from failing every run to passing every
  run, in nearly the clear-air time, and `slalom_fog` became a regression test.
  The full before and after is in [fog.md](../fog.md).
- A new obstacle is noticed two scans late: a fifth of a second, about 10 cm at
  0.5 m/s, well within the 1.5 m lookahead. A faster robot, or a shorter
  lookahead, would need that margin rechecked.
- Something real that is seen only intermittently, such as a thin object
  between beams, could be filtered out. The 0.15 m radius tolerates noise and
  small movements of the robot, but not a return that keeps disappearing.
- A moving obstacle that travels more than 0.15 m between scans, 1.5 m/s,
  would be filtered out entirely. There are none in these scenarios; a world
  with people or vehicles in it would need tracking rather than persistence.
- Smoothing costs about half a second on the slalom in clear air.
- The fog model's echoes are independent from scan to scan, which suits this
  filter. Real fog has denser patches that may persist briefly, so a real
  sensor would likely leak more clutter through.

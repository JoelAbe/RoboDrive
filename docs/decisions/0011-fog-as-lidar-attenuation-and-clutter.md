# 0011. Model fog as attenuation and clutter on the lidar

- **Status:** Accepted

## Context

Phase 4 needs the autonomy to fail for a documented, physical reason, and fog
is the case named in the roadmap. That needs a fog model the lidar responds to.

Unreal's own fog is a rendering effect. Line traces, which the lidar uses, do
not see it, so it changes how the level looks and nothing the robot senses.

Random beam dropouts already exist, but they are not what fog does to a lidar.
Fog does not make returns vanish at random. It weakens returns from distant
surfaces, and it reflects some light back from the fog itself, so the sensor
reports targets that are not there. Both effects are well documented in tests
of real lidars in fog chambers.

A full model of light scattering in fog would be far more than this project
needs.

## Decision

Fog is described by **meteorological visibility**, the quantity a weather report
gives, and converted to an extinction coefficient by Koschmieder's law. Two
effects follow from that coefficient:

- **Attenuation.** A return is detectable if, after spreading with the square
  of distance and two-way attenuation through the fog, it is at least as
  strong as the return from a target at the lidar's maximum range in clear air.
- **Clutter.** Each beam may return an echo from the fog itself. The distance to
  it is drawn from an exponential distribution whose rate is the extinction
  coefficient times a per-sensor **fog echo fraction**. The echo is reported
  only if it comes before any surface and is itself detectable.

The sensor reports the first echo, as a single-return lidar does. Visibility
lives in a weather subsystem in `RoboSim`, set by the scenario through
`visibility_m` and read by the lidar. A scenario without it clears the air.

## Consequences

- In clear air the lidar is unchanged, and no extra random number is drawn, so
  runs without fog reproduce exactly as before fog existed.
- One number, visibility, describes the fog, and it can be estimated by eye.
- The fog echo fraction is a sensor property that is assumed, not calibrated
  against a specific lidar. Changing it moves the visibility at which the
  robot fails, not whether it fails. The `set_lidar` API operation changes it
  at runtime, so the two effects can be measured apart.
- Only the lidar is affected. Wheel encoders and the IMU are not, which is
  physically right. The camera is not either, which is not: a foggy image would
  need rendering changes.
- The level does not look foggy. Fog echoes are drawn in a different colour
  from real returns so a person watching can see them, using knowledge only the
  simulation has.
- Echoes are independent from beam to beam and scan to scan. Real fog has
  structure, so this model may make clutter easier to filter out than it is in
  reality.
- The measurements are in [fog.md](../fog.md).

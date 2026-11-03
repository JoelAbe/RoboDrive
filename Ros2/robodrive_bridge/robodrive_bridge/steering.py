"""The goal driver's control law: head for the goal, stop short of obstacles.

Deliberately minimal. Its job is to show a ROS node driving the robot through
the bridge, not to navigate; it stops rather than steers around anything. The
in-engine avoiding brain, or Nav2, is where avoidance belongs.

Pure Python, so it can be tested without ROS.
"""

from __future__ import annotations

import math
from dataclasses import dataclass
from typing import Optional, Sequence

from robodrive_bridge.conversions import wrap_angle


@dataclass
class SteeringSettings:
    max_speed_ms: float = 0.5
    max_turn_rate_rads: float = 1.0
    turn_gain: float = 1.5
    goal_tolerance_m: float = 0.3
    slow_radius_m: float = 1.0
    stop_distance_m: float = 0.6
    ahead_half_angle_rad: float = 0.4


@dataclass
class Scan:
    angle_min: float
    angle_increment: float
    ranges: Sequence[float]


def nearest_ahead(scan: Scan, half_angle_rad: float) -> float:
    """The closest return within a cone ahead, or +inf if there is none."""
    nearest = math.inf
    for index, range_m in enumerate(scan.ranges):
        angle = scan.angle_min + index * scan.angle_increment
        if abs(wrap_angle(angle)) <= half_angle_rad:
            nearest = min(nearest, range_m)
    return nearest


def drive_towards(x: float, y: float, yaw: float, goal_x: float, goal_y: float,
                  scan: Optional[Scan], settings: SteeringSettings) -> Optional[tuple[float, float]]:
    """(linear, angular) to drive towards the goal, or None once there."""
    dx = goal_x - x
    dy = goal_y - y
    distance = math.hypot(dx, dy)

    if distance <= settings.goal_tolerance_m:
        return None

    heading_error = wrap_angle(math.atan2(dy, dx) - yaw)

    angular = max(-settings.max_turn_rate_rads,
                  min(settings.max_turn_rate_rads, settings.turn_gain * heading_error))

    # Full speed only when facing the goal, none when facing away, so it
    # turns on the spot first. Slower near the goal, so it does not overshoot.
    linear = (settings.max_speed_ms
              * max(0.0, math.cos(heading_error))
              * min(1.0, distance / settings.slow_radius_m))

    if scan is not None and nearest_ahead(scan, settings.ahead_half_angle_rad) < settings.stop_distance_m:
        linear = 0.0

    return linear, angular

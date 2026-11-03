"""Conversions between the simulation's API and the conventions of ROS messages.

Pure Python with no ROS imports, so they can be tested anywhere. The API
already uses REP-103 units and axes, so nothing here changes a frame: these
are only differences of representation.
"""

from __future__ import annotations

import math
from dataclasses import dataclass
from typing import Any, Optional, Sequence

NANOSECONDS_PER_SECOND = 1_000_000_000


def split_seconds(seconds: float) -> tuple[int, int]:
    """Simulation seconds as the (sec, nanosec) pair a ROS time stamp holds."""
    whole = math.floor(seconds)
    nanoseconds = int(round((seconds - whole) * NANOSECONDS_PER_SECOND))

    # Rounding can carry a whole second, as 0.9999999999 s does.
    if nanoseconds >= NANOSECONDS_PER_SECOND:
        whole += 1
        nanoseconds -= NANOSECONDS_PER_SECOND

    return int(whole), nanoseconds


def yaw_to_quaternion(yaw: float) -> tuple[float, float, float, float]:
    """A rotation about +Z, as (x, y, z, w)."""
    return 0.0, 0.0, math.sin(0.5 * yaw), math.cos(0.5 * yaw)


def ranges_for_ros(ranges: Sequence[Optional[float]]) -> list[float]:
    """Lidar ranges with "no return" as +inf, which REP 117 reserves for it.

    The API sends no return as null rather than as the maximum range, because
    "saw nothing" is not "something at the limit". +inf keeps that distinction
    in ROS, where a reading at range_max would be drawn as a wall.
    """
    return [math.inf if value is None else float(value) for value in ranges]


def as_floats(value: Any) -> Any:
    """A reply from the API with every number made a float.

    JSON does not distinguish 12 from 12.0, so a range limit of exactly 12 m
    arrives as an int. Assigned to a float field of a ROS message, an int is
    accepted without complaint and then published as 0.0.
    """
    if isinstance(value, dict):
        return {key: as_floats(item) for key, item in value.items()}
    if isinstance(value, list):
        return [as_floats(item) for item in value]
    if isinstance(value, int) and not isinstance(value, bool):
        return float(value)
    return value


def wrap_angle(angle: float) -> float:
    """An angle in (-pi, pi]."""
    return math.atan2(math.sin(angle), math.cos(angle))


@dataclass
class HeldCommand:
    """The latest velocity command from ROS, and when it arrived.

    The simulation stops the robot if commands stop arriving, and the bridge
    must not defeat that by repeating a command forever after its sender has
    gone. A command is forwarded only while it is fresh, judged in simulation
    time so the rule holds however fast the simulation runs.
    """

    timeout_s: float
    linear: float = 0.0
    angular: float = 0.0
    received_at: Optional[float] = None

    def set(self, linear: float, angular: float, sim_time: float) -> None:
        self.linear = linear
        self.angular = angular
        self.received_at = sim_time

    def current(self, sim_time: float) -> Optional[tuple[float, float]]:
        """The command to forward now, or None if there is no fresh one."""
        if self.received_at is None or sim_time - self.received_at > self.timeout_s:
            return None
        return self.linear, self.angular

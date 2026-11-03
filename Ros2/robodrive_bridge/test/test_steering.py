import math

from robodrive_bridge.steering import Scan, SteeringSettings, drive_towards, nearest_ahead

SETTINGS = SteeringSettings()


def scan_with(ranges_by_angle: dict[float, float]) -> Scan:
    """A scan of beams every 0.1 rad from -1.5 to 1.5, empty but for the given ones."""
    angles = [round(-1.5 + 0.1 * index, 1) for index in range(31)]
    return Scan(-1.5, 0.1, [ranges_by_angle.get(angle, math.inf) for angle in angles])


def test_drives_straight_at_a_goal_ahead():
    linear, angular = drive_towards(0.0, 0.0, 0.0, 5.0, 0.0, None, SETTINGS)
    assert linear == SETTINGS.max_speed_ms
    assert angular == 0.0


def test_turns_on_the_spot_towards_a_goal_behind():
    linear, angular = drive_towards(0.0, 0.0, 0.0, -5.0, 0.1, None, SETTINGS)
    assert linear == 0.0
    assert angular == SETTINGS.max_turn_rate_rads


def test_turns_right_for_a_goal_to_the_right():
    _, angular = drive_towards(0.0, 0.0, 0.0, 5.0, -1.0, None, SETTINGS)
    assert angular < 0.0


def test_slows_near_the_goal():
    linear, _ = drive_towards(0.0, 0.0, 0.0, 0.5, 0.0, None, SETTINGS)
    assert 0.0 < linear < SETTINGS.max_speed_ms


def test_arrives_within_tolerance():
    assert drive_towards(0.0, 0.0, 0.0, 0.2, 0.0, None, SETTINGS) is None


def test_stops_for_an_obstacle_ahead_but_keeps_turning():
    scan = scan_with({0.0: 0.4})
    linear, angular = drive_towards(0.0, 0.0, 0.0, 5.0, 1.0, scan, SETTINGS)
    assert linear == 0.0
    assert angular > 0.0


def test_ignores_obstacles_off_to_the_side():
    scan = scan_with({1.2: 0.3, -1.2: 0.3})
    linear, _ = drive_towards(0.0, 0.0, 0.0, 5.0, 0.0, scan, SETTINGS)
    assert linear == SETTINGS.max_speed_ms


def test_nearest_ahead():
    assert nearest_ahead(scan_with({}), 0.4) == math.inf
    assert nearest_ahead(scan_with({0.3: 2.0, -0.2: 1.0, 1.0: 0.1}), 0.4) == 1.0

import math

from robodrive_bridge.conversions import (HeldCommand, as_floats, ranges_for_ros, split_seconds, wrap_angle,
                                          yaw_to_quaternion)


def test_split_seconds():
    assert split_seconds(0.0) == (0, 0)
    assert split_seconds(12.25) == (12, 250_000_000)


def test_split_seconds_carries_a_rounded_up_second():
    # Nanoseconds that round up to a whole second must carry into seconds,
    # never come out as 1,000,000,000, which a ROS stamp would reject.
    assert split_seconds(0.9999999999) == (1, 0)


def test_yaw_to_quaternion():
    assert yaw_to_quaternion(0.0) == (0.0, 0.0, 0.0, 1.0)

    x, y, z, w = yaw_to_quaternion(math.pi / 2)
    assert (x, y) == (0.0, 0.0)
    assert math.isclose(z, math.sqrt(0.5))
    assert math.isclose(w, math.sqrt(0.5))


def test_no_return_becomes_infinity():
    assert ranges_for_ros([1.5, None, 0.2]) == [1.5, math.inf, 0.2]


def test_whole_numbers_become_floats():
    reply = as_floats({'range_max': 12, 'ranges': [1, None, 2.5], 'ok': True, 'name': 'x'})
    assert reply == {'range_max': 12.0, 'ranges': [1.0, None, 2.5], 'ok': True, 'name': 'x'}
    assert isinstance(reply['range_max'], float)
    assert isinstance(reply['ranges'][0], float)
    assert reply['ok'] is True


def test_wrap_angle():
    assert math.isclose(wrap_angle(3 * math.pi / 2), -math.pi / 2)
    assert math.isclose(wrap_angle(-3 * math.pi / 2), math.pi / 2)


def test_held_command_is_forwarded_only_while_fresh():
    command = HeldCommand(timeout_s=0.5)
    assert command.current(10.0) is None

    command.set(0.4, -0.2, sim_time=10.0)
    assert command.current(10.5) == (0.4, -0.2)
    assert command.current(10.51) is None

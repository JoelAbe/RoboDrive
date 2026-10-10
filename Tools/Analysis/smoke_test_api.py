"""Smoke test for the RoboDrive API.

Checks each operation, then drives the robot in a client-controlled loop: send
a command, advance a fixed number of simulation steps, repeat.

    python Tools/analysis/smoke_test_api.py

The simulation must be playing: the API only exists while a game world does.
"""

from __future__ import annotations

import argparse

from robodrive_client import DEFAULT_HOST, DEFAULT_PORT, ApiError, SimClient


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", default=DEFAULT_HOST)
    parser.add_argument("--port", type=int, default=DEFAULT_PORT)
    parser.add_argument("--seconds", type=float, default=10.0, help="how long to drive")
    args = parser.parse_args()

    with SimClient(args.host, args.port) as client:
        print("ping      ", client.call("ping"))
        print("get_state ", client.call("get_state"))

        try:
            client.call("frobnicate")
            print("FAIL: an unknown operation was accepted")
        except ApiError as error:
            print("rejected  ", error)

        print("reset     ", client.call("reset"))

        sensors = client.call("get_sensors", lidar_stride=30)
        print("odometry  ", sensors["odometry"])
        print("imu accel ", [round(value, 2) for value in sensors["imu"]["accel"]])
        print("lidar     ", sensors["lidar"]["ranges"])
        print("truth     ", client.call("get_ground_truth")["pose"])

        # Re-send the command every cycle: commands expire after 0.25 s of
        # simulation time, so a client that stops sending gets a robot that
        # stops moving.
        for _ in range(int(args.seconds * 10)):
            client.call("set_command", v=0.5, w=0.25)
            client.call("step", steps=12)

        state = client.call("get_state")
        print("get_state ", state)


if __name__ == "__main__":
    main()
"""Smoke test for the RoboDrive API.

Connects to the simulation, checks each operation, then drives the robot in a
client-controlled loop: send a command, advance a fixed number of simulation
steps, repeat. That loop is the shape every automated test will take.

    python Tools/analysis/smoke_test_api.py
    python Tools/analysis/smoke_test_api.py --host 127.0.0.1 --port 8765

The simulation must be playing: the API only exists while a game world does.
"""

from __future__ import annotations

import argparse
import itertools
import json
import os
import socket
from typing import Any, Iterator, TextIO


class ApiError(RuntimeError):
    """The simulation refused a request."""


class SimClient:
    """A connection to the simulation.

    Requests and responses are JSON, one per line. Each request carries an id
    that the simulation echoes back, which is what lets a client match a reply
    to the request that caused it.
    """

    def __init__(self, connection: socket.socket) -> None:
        self._connection = connection
        self._reader: TextIO = connection.makefile("r", encoding="utf-8")
        self._ids: Iterator[int] = itertools.count(1)

    def call(self, operation: str, **fields: Any) -> dict:
        """Send one request and return its result, raising if it failed."""
        request_id = next(self._ids)
        request = {"id": request_id, "op": operation, **fields}

        self._connection.sendall((json.dumps(request) + "\n").encode("utf-8"))

        line = self._reader.readline()
        if not line:
            raise ApiError("the simulation closed the connection")

        response = json.loads(line)

        if response.get("id") != request_id:
            raise ApiError(f"expected a reply to {request_id}, got {response.get('id')}")

        if not response.get("ok"):
            raise ApiError(response.get("error", "unknown error"))

        return response.get("result", {})


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", default=os.environ.get("ROBODRIVE_HOST", "127.0.0.1"))
    parser.add_argument("--port", type=int, default=int(os.environ.get("ROBODRIVE_PORT", "8765")))
    parser.add_argument("--seconds", type=float, default=10.0, help="how long to drive")
    args = parser.parse_args()

    # Generous timeout
    with socket.create_connection((args.host, args.port), timeout=30) as connection:
        client = SimClient(connection)

        print("ping      ", client.call("ping"))
        print("get_state ", client.call("get_state"))

        # An API that fails clearly matters as much as one that works, so check
        # that a nonsense request is refused rather than ignored.
        try:
            client.call("frobnicate")
            print("FAIL: an unknown operation was accepted")
        except ApiError as error:
            print("rejected  ", error)

        print("reset     ", client.call("reset"))

        # Drive at 10 Hz: one command, then 12 steps of 1/120 s, repeated.
        commands_per_second = 10
        steps_per_command = 12

        for _ in range(int(args.seconds * commands_per_second)):
            client.call("set_command", v=0.5, w=0.25)
            client.call("step", steps=steps_per_command)

        state = client.call("get_state")
        print("get_state ", state)
        print(f"\ndrove for {args.seconds:.0f} s of simulation time, "
              f"ending at step {state['step']}")


if __name__ == "__main__":
    main()
"""Client for the RoboDrive simulation API.

Requests and responses are JSON, one per line, over a TCP socket. Each request
carries an id that the simulation echoes back, which is what lets a client
match a reply to the request that caused it.

    from robodrive_client import SimClient

    with SimClient() as client:
        client.call("reset")
        client.call("set_command", v=0.5, w=0.0)
        client.call("step", steps=12)
"""

from __future__ import annotations

import itertools
import json
import os
import socket
from typing import Any, Iterator, TextIO

DEFAULT_HOST = os.environ.get("ROBODRIVE_HOST", "127.0.0.1")
DEFAULT_PORT = int(os.environ.get("ROBODRIVE_PORT", "8765"))


class ApiError(RuntimeError):
    """The simulation refused a request."""


class SimClient:
    """A connection to the simulation. Use it as a context manager so the
    socket is always closed, even if a test fails part way through."""

    def __init__(self, host: str = DEFAULT_HOST, port: int = DEFAULT_PORT,
                 timeout: float = 30.0) -> None:
        # Generous timeout: a step request does not reply until the simulation
        # has advanced, which takes longer than a round trip.
        self._connection = socket.create_connection((host, port), timeout=timeout)
        # A file object over the socket gives readline(), which is exactly the
        # framing the protocol uses.
        self._reader: TextIO = self._connection.makefile("r", encoding="utf-8")
        self._ids: Iterator[int] = itertools.count(1)

    def __enter__(self) -> "SimClient":
        return self

    def __exit__(self, *exc_info: object) -> None:
        self.close()

    def close(self) -> None:
        self._reader.close()
        self._connection.close()

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
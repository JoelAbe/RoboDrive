"""Smoke test for the RoboDrive API: connect, ping, read state.

    python Tools/analysis/smoke_test_api.py
"""

import argparse
import json
import os
import socket

HOST, PORT = "127.0.0.1", 8765


def request(connection: socket.socket, reader, operation: str, request_id: int) -> dict:
    connection.sendall((json.dumps({"id": request_id, "op": operation}) + "\n").encode())
    return json.loads(reader.readline())


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", default=os.environ.get("ROBODRIVE_HOST", "127.0.0.1"))
    parser.add_argument("--port", type=int, default=int(os.environ.get("ROBODRIVE_PORT", "8765")))
    args = parser.parse_args()

    with socket.create_connection((args.host, args.port), timeout=5) as connection:
        reader = connection.makefile("r", encoding="utf-8")
        print(request(connection, reader, "ping", 1))
        print(request(connection, reader, "get_state", 2))
        print(request(connection, reader, "frobnicate", 3))


if __name__ == "__main__":
    main()
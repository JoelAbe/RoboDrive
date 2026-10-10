"""Load a scenario file into the running simulation.

    python Tools/analysis/load_scenario.py Scenarios/corridor.json

Reads the file here and sends its contents, so the simulation never needs
access to this machine's filesystem.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path

from robodrive_client import DEFAULT_HOST, DEFAULT_PORT, SimClient


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("scenario", type=Path, help="path to a scenario JSON file")
    parser.add_argument("--host", default=DEFAULT_HOST)
    parser.add_argument("--port", type=int, default=DEFAULT_PORT)
    args = parser.parse_args()

    scenario = json.loads(args.scenario.read_text(encoding="utf-8"))

    with SimClient(args.host, args.port) as client:
        result = client.call("load_scenario", scenario=scenario)
        print(f"loaded '{result['name']}' with {result['obstacles']} obstacles")


if __name__ == "__main__":
    main()
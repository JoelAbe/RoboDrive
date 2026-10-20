# 0015. Load scenarios in the simulation, read scenario files in the client

- **Status:** Accepted

## Context

Scenarios were built by the external API module: `load_scenario` validated the
JSON, placed obstacles, moved the robot and set the goal and weather. The
operator view then needed to load scenarios too, so an operator could pick one
from a list and restart it.

The operator view lives in the game module, which cannot depend on the API
module. That module deliberately has nothing depending on it
([ADR 0007](0007-external-api-module.md)).

## Decision

Building a world from a scenario moves into a scenario subsystem in `RoboSim`,
alongside the mission and the weather. Both the API and the operator view call
it.

- It is handed a scenario's **contents**, never a path. Reading files remains
  the client's job: the Python harness reads them and sends them over the API,
  and the operator view reads the project's `Scenarios/` folder itself
  ([ADR 0008](0008-scenarios-as-data.md)).
- It **remembers** the last scenario that loaded, so it can be restarted, and
  can **unload** it, leaving the level as it was built.
- Validation is a static function that needs no world, so it is unit tested
  directly.

## Consequences

- The API and the operator view share one implementation, so a scenario loaded
  either way behaves identically, and each sees what the other loaded.
- The API gains `restart_scenario` and `unload_scenario`, and `get_state`
  reports the current scenario's name.
- Restarting reapplies the scenario as written, undoing any change to the goal
  or weather made since.
- Simulation time is never rewound by a restart, for the same reason as
  `reset`: sensors have scheduled their next samples against it.
- `RoboSim` now depends on the engine's `Json` module, privately.
- A packaged build would need `Scenarios/` staged alongside it, as a
  non-asset directory, for the operator view to find any files.

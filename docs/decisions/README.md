# Architecture Decision Records

Each file records one decision: the context that forced it, what was decided,
and the consequences, including the downsides. Records are not rewritten after
the fact; a changed decision gets a new record that supersedes the old one.

Decisions are recorded when their code lands, not when they are first
discussed, so this list stays a description of the project rather than a plan.

| # | Decision | Status |
|---|---|---|
| [0001](0001-three-module-split.md) | Split the C++ code into three modules | Accepted |
| [0002](0002-rep103-internal-frames.md) | Use REP-103 frames internally | Accepted |
| [0003](0003-velocity-command-interface.md) | Command the robot with (linear velocity, angular velocity) | Accepted |
| [0004](0004-raycast-wheel-vehicle-model.md) | Model the vehicle with raycast wheels and casters | Accepted |
| [0005](0005-arbitration-by-priority-and-freshness.md) | Arbitrate drive commands by priority and freshness | Accepted |
| [0006](0006-physics-on-the-actor-tick.md) | Keep physics forces on the actor tick | Accepted |
| [0007](0007-external-api-module.md) | Expose the simulation through a TCP API in its own module | Accepted |
| [0008](0008-scenarios-as-data.md) | Define scenarios as JSON, judged by the test harness | Accepted |
| [0009](0009-brains-receive-a-snapshot.md) | Give brains a sensor snapshot, never the robot | Accepted |
| [0010](0010-reactive-obstacle-avoidance.md) | Avoid obstacles reactively, layered on go-to-goal | Accepted |
| [0011](0011-fog-as-lidar-attenuation-and-clutter.md) | Model fog as attenuation and clutter on the lidar | Accepted |
| [0012](0012-headless-runs-on-a-fixed-time-step.md) | Run headless simulations on a fixed time step | Accepted |
| [0013](0013-operator-modes-and-emergency-stop.md) | Make modes a gate and the emergency stop a latch | Accepted |
| [0014](0014-operator-view-in-slate.md) | Build the operator view in Slate, fed by a telemetry snapshot | Accepted |

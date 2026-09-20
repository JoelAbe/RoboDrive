# 0001. Split the C++ code into three modules

- **Status:** Accepted

## Context

The robot has to be drivable by several different sources: keyboard input, an
in-engine autonomy, external test scripts, and eventually a ROS 2 stack.
Swapping one for another should not require changing the vehicle or sensor
code.

In Unreal, each C++ module compiles to its own DLL and must declare which other
modules it depends on. A module can only include headers from modules it
declares, so the build system can enforce a dependency direction that would
otherwise be a convention.

## Decision

Three runtime modules, each depending only on those below it:

- **RoboSim** — vehicle, sensors, odometry. Depends on no project module.
- **RoboAutonomy** — the in-engine brain. Depends on RoboSim, publicly,
  because its public headers will expose RoboSim types.
- **RoboDrive** — the game module. Depends on both, privately, because nothing
  depends on RoboDrive.

## Consequences

- The dependency direction is enforced by the compiler. Including a
  RoboAutonomy header from RoboSim fails with "cannot open include file".
- An external autonomy source can be added without touching RoboSim, because
  RoboSim has no way to refer to a brain in the first place.
- Types shared between the brain and the body, such as drive commands and
  sensor data, have to live in RoboSim.
- Classes used across module boundaries need the module's `_API` export macro.
  Forgetting it produces linker errors rather than compiler errors, which are
  harder to recognise.
- Adding a module requires closing the editor and regenerating project files.
  Live Coding cannot add modules.

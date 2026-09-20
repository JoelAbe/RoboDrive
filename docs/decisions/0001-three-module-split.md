# 0001. Split the C++ code into three modules

- **Status:** Accepted

## Context

The robot has to be drivable by several different sources: keyboard input,
an in-engine autonomy, external test scripts, and eventually a ROS 2 stack.
Swapping one for another should not require changing the vehicle or sensor
code.

In Unreal, each C++ module compiles to its own DLL and has to declare which
other modules it depends on. A module can only include headers from modules
it declares.

## Decision

Three runtime modules, each depending only on those below it:

- **RoboSim**: vehicle, sensors, odometry. Depends on no project module.
- **RoboAutonomy**: the in-engine brain. Depends on RoboSim (public, because
  its public headers will expose RoboSim types).
- **RoboDrive**: the game module. Depends on both (private, because nothing
  depends on RoboDrive).

## Consequences

- The dependency direction is enforced by the compiler. Including a
  RoboAutonomy header from RoboSim fails with "cannot open include file".
- An external autonomy source can be added without touching RoboSim.
- Types shared between the brain and the body (drive commands, sensor data)
  have to live in RoboSim.
- Classes used across modules need the module's `_API` export macro.
  Forgetting it produces linker errors rather than compiler errors, which
  are harder to recognise.
- Adding a module requires closing the editor and regenerating project
  files; Live Coding cannot add modules.
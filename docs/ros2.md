# ROS 2

`Ros2/robodrive_bridge` connects the simulation to ROS 2. The bridge is just
another client of the [external API](api.md), like the scenario runner: it
publishes what the robot could know as standard ROS messages, including
simulation time on `/clock`, and forwards `/cmd_vel` back as drive commands.
The simulation itself knows nothing about ROS.
([ADR 0017](decisions/0017-ros2-bridge-as-an-api-client.md))

The package also has a small ROS node that drives the robot to a goal on
simulation time, and a launch file that starts both with RViz.

![The editor and RViz side by side. The operator view reports the robot is driven by an External source while the in-engine brain waits for a goal. RViz shows the odometry trail in red, turning on the spot and heading for a goal set with 2D Goal Pose, the true pose in green at its end, and the lidar returns from two walls ahead in orange](images/ros2-rviz.jpg)

The operator view says *Driving: External*: the robot is following the goal
driver's commands, sent through ROS, while the in-engine brain waits. In RViz
the red arrows are odometry's trail, with a fan where the robot turned on the
spot towards the goal, the green arrow is the true pose, and the orange squares
are the lidar's view of the two walls ahead.

## What it publishes

| Topic | Type | |
|---|---|---|
| `/clock` | `rosgraph_msgs/Clock` | Simulation time, every bridge cycle |
| `/scan` | `sensor_msgs/LaserScan` | Each new lidar scan, 10 Hz of simulation time. No return is `+inf` (REP 117). |
| `/odom` | `nav_msgs/Odometry` | Wheel odometry: the pose the encoders estimate, with its drift, and the velocity they measured |
| `/imu` | `sensor_msgs/Imu` | Accelerometer and gyro. No orientation, marked with a covariance of -1 (REP 145). |
| `/tf` | | `odom` → `base_link`, from odometry |
| `/tf_static` | | `base_link` → `laser` and `imu_link`, and `world` → `odom` when ground truth is on |
| `/ground_truth/pose` | `geometry_msgs/PoseStamped` | The true pose, only with `publish_ground_truth` |

It subscribes to `/cmd_vel` (`geometry_msgs/Twist`, or `TwistStamped` with
`cmd_vel_stamped:=true`, as Nav2 sends by default). A command is forwarded only
while it is fresh, half a second of simulation time, so a ROS node that stops
publishing stops the robot, as the simulation's own watchdog intends. While
commands are arriving they outrank the in-engine autonomy; when they stop,
control returns to it.

Every timestamp is simulation time. Nodes that should run on it, which is all
of them except the bridge, need `use_sim_time:=true`.

**Ground truth is for display.** It is published so RViz can show the true
pose beside odometry, and the gap between them is odometry's drift. Anything
that drives the robot must not subscribe to it, for the same reason the API
keeps it separate: autonomy that reads the truth works in simulation and fails
on a robot. The `world` → `odom` transform is identity because odometry starts
from the true pose at every reset, so their frames coincide.

## Installing ROS 2 on Windows

The bridge needs ROS 2 **Lyrical Luth**, the current long-term-support
release. Jazzy, the previous one, supports only Windows 10 and no longer ships
Windows builds. Lyrical supports Windows 11. The
[official guide](https://docs.ros.org/en/lyrical/Get-Started/Installation/Windows-Install-Binary.html)
is the reference; in short:

1. Install pixi, which ROS 2 uses on Windows to provide Python, Qt and its
   other dependencies:

   ```powershell
   winget install --id prefix-dev.pixi --exact
   ```

2. Download `ros2-lyrical-*-windows-AMD64.zip` from the
   [ROS 2 releases](https://github.com/ros2/ros2/releases), unzip it to
   `C:\dev`, and rename the `ros2-windows` folder it contains to `lyrical`. Keep
   the path short: Windows path limits bite otherwise.

3. Open a new terminal, so pixi is on the PATH, and install the dependencies,
   about 2 GB:

   ```powershell
   cd C:\dev\lyrical
   pixi install
   pixi run python preinstall_setup_windows.py
   ```

On Linux, install Lyrical the usual way and use `source` instead of the
PowerShell script below; the bridge is plain `rclpy` and has nothing specific
to Windows.

## Building and running

In each terminal that will use ROS, set it up first. `Tools/RosEnv.ps1` does
everything: it activates pixi's environment, sources ROS 2, and sources this
repository's ROS packages once they are built.

```powershell
. .\Tools\RosEnv.ps1
cd Ros2
colcon build
cd ..
. .\Tools\RosEnv.ps1
```

The second `RosEnv.ps1` picks up the packages just built. After that, one is
enough. Then press Play in the editor, and:

```powershell
ros2 launch robodrive_bridge robodrive.launch.py
```

RViz opens, centred on the robot. Choose **2D Goal Pose** in the toolbar and
click on the floor: the goal driver turns towards it and drives there. The red
arrows are where odometry believes the robot has been, the green arrow is where
it really is, and the orange squares are the lidar returns.

Launch arguments:

| Argument | Default | |
|---|---|---|
| `port` | `8765` | The API's port. The editor uses 8765. |
| `ground_truth` | `true` | Publish and draw the true pose |
| `driver` | `true` | Start the goal driver. Turn it off to drive from another node, Nav2 for instance. |
| `rviz` | `true` | Start RViz |

The bridge can be started before the simulation and waits for it, and
reconnects when Play is pressed again.

### The goal driver

`goal_driver` steers by odometry, the only pose the robot has, so goals must be
in the `odom` frame, which is what RViz sends. It turns towards the goal, drives
at up to 0.5 m/s, slows within a metre, and stops within 0.3 m. If the lidar
sees anything within 0.6 m ahead it stops and waits rather than steering
around it.

That is deliberate. It exists to show a ROS node driving the robot through the
bridge on simulation time; avoidance is what the in-engine brain or Nav2 is
for. To watch the robot avoid obstacles under ROS, give the goal with the
operator view instead: the in-engine brain drives, and ROS observes.

## Simulation time

Each bridge cycle reads the sensors, publishes them, forwards any fresh command,
then asks the simulation to advance four fixed steps, a thirtieth of a second,
and waits until it has. The bridge therefore runs on simulation time,
whatever the wall clock is doing. The goal driver's 10 Hz timer is 10 Hz of
`/clock`.

Measured, driving 8 m down the corridor with the goal driver:

| | In the editor, real time | Headless, fixed time step |
|---|---|---|
| Simulated seconds per real second | 1.0 | 5.7 |
| Bridge cycles per simulated second | 30.0 | 14.1 |
| Lidar scans per simulated second | 9.9 | 9.9 |
| Where odometry put the robot when it stopped | 7.72 m | 7.75 m |
| Where it really was | 7.69 m | 7.73 m |

The goal was at 8 m with a 0.3 m tolerance, so both stopped just inside it.

The robot behaves the same at both speeds. What changes is how finely the
bridge samples: the API is advance-and-wait, not lockstep, so a headless
simulation does not wait for the bridge between cycles
([ADR 0012](decisions/0012-headless-runs-on-a-fixed-time-step.md)). Every
lidar scan still arrives, because each one is published once, whenever the
bridge first sees it.

An early version of the bridge queued up to ten commands and handled one per
cycle. Headless, with the bridge cycling barely faster than the driver sent,
the queue fell behind, old commands were replayed late, and the robot overshot
the goal by half a metre. Velocity commands now have a queue of one: only the
newest matters.

## Tests

The conversions and the goal driver's control law are plain Python, with
tests that need no ROS running:

```powershell
. .\Tools\RosEnv.ps1
cd Ros2\robodrive_bridge
python -m pytest test
```

They are not in CI, because the CI runner has no ROS 2 installed.

## Troubleshooting

- **RViz fails with "Could not find the Qt platform plugin".** The terminal
  was set up with pixi but not with `RosEnv.ps1`, which works around pixi
  writing the plugin path in cmd.exe syntax.
- **The bridge logs "No simulation at 127.0.0.1:8765".** The editor is not
  playing, or something else is using the API: it serves one client at a time,
  so the scenario runner and the bridge cannot run against the same
  simulation at once.
- **`ros2 node list` or `ros2 topic list` hangs.** The ROS command-line daemon
  can stall on Windows. Add `--no-daemon`.
- **"Cannot create a symlink to latest log directory".** Harmless. Windows
  needs Developer Mode for symlinks, and ROS uses one only to point at the
  latest log.

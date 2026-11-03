"""Bridges the RoboDrive simulation to ROS 2.

Just another client of the simulation's TCP API, like the scenario runner: it
reads what the robot could know, publishes it as standard ROS messages, and
forwards velocity commands back. The simulation knows nothing about ROS.

Published:
    /clock                rosgraph_msgs/Clock      simulation time
    /scan                 sensor_msgs/LaserScan    once per new lidar scan
    /odom                 nav_msgs/Odometry        wheel odometry, with its drift
    /imu                  sensor_msgs/Imu          accelerometer and gyro
    /tf                   odom -> base_link
    /tf_static            base_link -> laser, base_link -> imu_link
    /ground_truth/pose    geometry_msgs/PoseStamped, only if publish_ground_truth

Subscribed:
    /cmd_vel              geometry_msgs/Twist, or TwistStamped if cmd_vel_stamped

Each cycle reads the sensors, publishes, forwards any fresh command, then asks
the simulation to advance a few fixed steps and waits for it. The bridge
therefore runs at a steady rate in simulation time, whether the simulation is
running in real time in the editor or many times faster headless.
"""

from __future__ import annotations

import time

import rclpy
from builtin_interfaces.msg import Time
from geometry_msgs.msg import PoseStamped, TransformStamped, Twist, TwistStamped
from nav_msgs.msg import Odometry
from rclpy.node import Node
from rosgraph_msgs.msg import Clock
from sensor_msgs.msg import Imu, LaserScan
from tf2_ros import StaticTransformBroadcaster, TransformBroadcaster

from robodrive_bridge.conversions import HeldCommand, as_floats, ranges_for_ros, split_seconds, yaw_to_quaternion
from robodrive_bridge.sim_client import ApiError, SimClient

# The simulation's fixed step, s.
SIM_STEP_S = 1.0 / 120.0

# Where the sensors sit on the robot, m, from the vehicle's components.
LIDAR_HEIGHT_M = 0.15


def stamp(seconds: float) -> Time:
    sec, nanosec = split_seconds(seconds)
    return Time(sec=sec, nanosec=nanosec)


class Bridge(Node):

    def __init__(self) -> None:
        # The bridge is the source of simulation time, so it never uses it.
        super().__init__('robodrive_bridge')

        self.host = self.declare_parameter('host', '127.0.0.1').value
        self.port = self.declare_parameter('port', 8765).value
        self.steps_per_cycle = self.declare_parameter('steps_per_cycle', 4).value
        self.publish_ground_truth = self.declare_parameter('publish_ground_truth', False).value
        self.odom_frame = self.declare_parameter('odom_frame', 'odom').value
        self.base_frame = self.declare_parameter('base_frame', 'base_link').value
        self.laser_frame = self.declare_parameter('laser_frame', 'laser').value
        self.imu_frame = self.declare_parameter('imu_frame', 'imu_link').value
        self.world_frame = self.declare_parameter('world_frame', 'world').value

        # Twice the simulation's own command lifetime, so a command sent at
        # 10 Hz is never judged stale between two of the bridge's cycles.
        self.command = HeldCommand(timeout_s=self.declare_parameter('command_timeout_s', 0.5).value)

        self.clock_publisher = self.create_publisher(Clock, '/clock', 10)
        self.scan_publisher = self.create_publisher(LaserScan, 'scan', 10)
        self.odom_publisher = self.create_publisher(Odometry, 'odom', 10)
        self.imu_publisher = self.create_publisher(Imu, 'imu', 10)
        self.truth_publisher = self.create_publisher(PoseStamped, 'ground_truth/pose', 10)
        self.tf_broadcaster = TransformBroadcaster(self)
        self.static_tf_broadcaster = StaticTransformBroadcaster(self)

        # A queue of one: only the newest command matters. A deeper queue lets
        # the bridge fall behind, when it runs little faster than the commands
        # arrive, and play old commands back late; the robot then overshoots.
        if self.declare_parameter('cmd_vel_stamped', False).value:
            self.create_subscription(TwistStamped, 'cmd_vel', lambda msg: self.on_command(msg.twist), 1)
        else:
            self.create_subscription(Twist, 'cmd_vel', self.on_command, 1)

        self.sim_time = 0.0
        self.last_scan_time = None
        self.last_scan_interval = 0.1
        self.last_imu_time = None

    def on_command(self, twist: Twist) -> None:
        # Stamped with the latest simulation time the bridge has seen, which is
        # when, as far as the simulation is concerned, the command arrived.
        self.command.set(twist.linear.x, twist.angular.z, self.sim_time)

    def run(self) -> None:
        """Connect, bridge until the connection drops, and try again.

        The simulation's API exists only while a world is running, so the
        bridge can be started first and will wait for it.
        """
        warned = False
        while rclpy.ok():
            try:
                with SimClient(self.host, self.port) as client:
                    self.get_logger().info(f'Connected to the simulation at {self.host}:{self.port}')
                    warned = False
                    self.publish_static_transforms()
                    while rclpy.ok():
                        rclpy.spin_once(self, timeout_sec=0.0)
                        self.cycle(client)
            except (OSError, ApiError) as error:
                if not warned:
                    self.get_logger().warning(
                        f'No simulation at {self.host}:{self.port} ({error}). '
                        'Waiting for one: press Play in the editor, or start it headless.')
                    warned = True
                # Wall-clock time: there is no simulation to measure it by.
                time.sleep(1.0)

    def cycle(self, client: SimClient) -> None:
        sensors = as_floats(client.call('get_sensors'))
        self.sim_time = sensors['sim_time']

        self.clock_publisher.publish(Clock(clock=stamp(self.sim_time)))

        if 'odometry' in sensors:
            self.publish_odometry(sensors['odometry'])
        if 'imu' in sensors:
            self.publish_imu(sensors['imu'])
        if 'lidar' in sensors:
            self.publish_scan(sensors['lidar'])
        if self.publish_ground_truth:
            self.publish_truth(as_floats(client.call('get_ground_truth')))

        command = self.command.current(self.sim_time)
        if command is not None:
            client.call('set_command', v=command[0], w=command[1])

        client.call('step', steps=self.steps_per_cycle)

    def publish_static_transforms(self) -> None:
        def mounted(child: str, z: float) -> TransformStamped:
            transform = TransformStamped()
            transform.header.stamp = stamp(self.sim_time)
            transform.header.frame_id = self.base_frame
            transform.child_frame_id = child
            transform.transform.translation.z = z
            transform.transform.rotation.w = 1.0
            return transform

        transforms = [mounted(self.laser_frame, LIDAR_HEIGHT_M), mounted(self.imu_frame, 0.0)]

        # Odometry starts from the true pose at every reset, so its frame
        # coincides with the world's. Ground truth is drawn in the world frame,
        # and the gap between it and the robot is odometry's drift.
        if self.publish_ground_truth:
            world = TransformStamped()
            world.header.stamp = stamp(self.sim_time)
            world.header.frame_id = self.world_frame
            world.child_frame_id = self.odom_frame
            world.transform.rotation.w = 1.0
            transforms.append(world)

        self.static_tf_broadcaster.sendTransform(transforms)

    def publish_odometry(self, odometry: dict) -> None:
        header_stamp = stamp(odometry['time'])
        qx, qy, qz, qw = yaw_to_quaternion(odometry['yaw'])

        message = Odometry()
        message.header.stamp = header_stamp
        message.header.frame_id = self.odom_frame
        message.child_frame_id = self.base_frame
        message.pose.pose.position.x = odometry['x']
        message.pose.pose.position.y = odometry['y']
        message.pose.pose.orientation.x = qx
        message.pose.pose.orientation.y = qy
        message.pose.pose.orientation.z = qz
        message.pose.pose.orientation.w = qw
        # The velocity the encoders measured, in the robot's own frame.
        message.twist.twist.linear.x = odometry['v']
        message.twist.twist.angular.z = odometry['w']
        self.odom_publisher.publish(message)

        transform = TransformStamped()
        transform.header.stamp = header_stamp
        transform.header.frame_id = self.odom_frame
        transform.child_frame_id = self.base_frame
        transform.transform.translation.x = odometry['x']
        transform.transform.translation.y = odometry['y']
        transform.transform.rotation = message.pose.pose.orientation
        self.tf_broadcaster.sendTransform(transform)

    def publish_imu(self, imu: dict) -> None:
        if imu['time'] == self.last_imu_time:
            return
        self.last_imu_time = imu['time']

        message = Imu()
        message.header.stamp = stamp(imu['time'])
        message.header.frame_id = self.imu_frame
        # The IMU measures no orientation. REP 145: say so with -1.
        message.orientation_covariance[0] = -1.0
        message.linear_acceleration.x, message.linear_acceleration.y, message.linear_acceleration.z = imu['accel']
        message.angular_velocity.x, message.angular_velocity.y, message.angular_velocity.z = imu['gyro']
        self.imu_publisher.publish(message)

    def publish_scan(self, lidar: dict) -> None:
        if lidar['time'] == self.last_scan_time:
            return
        if self.last_scan_time is not None and lidar['time'] > self.last_scan_time:
            self.last_scan_interval = lidar['time'] - self.last_scan_time
        self.last_scan_time = lidar['time']

        ranges = ranges_for_ros(lidar['ranges'])

        message = LaserScan()
        message.header.stamp = stamp(lidar['time'])
        message.header.frame_id = self.laser_frame
        message.angle_min = lidar['angle_min']
        message.angle_increment = lidar['angle_increment']
        message.angle_max = lidar['angle_min'] + lidar['angle_increment'] * (len(ranges) - 1)
        # The simulated scan is taken all at once, not swept beam by beam.
        message.time_increment = 0.0
        message.scan_time = self.last_scan_interval
        message.range_min = lidar['range_min']
        message.range_max = lidar['range_max']
        message.ranges = ranges
        self.scan_publisher.publish(message)

    def publish_truth(self, truth: dict) -> None:
        pose = truth['pose']
        qx, qy, qz, qw = yaw_to_quaternion(pose['yaw'])

        message = PoseStamped()
        message.header.stamp = stamp(truth['sim_time'])
        message.header.frame_id = self.world_frame
        message.pose.position.x = pose['x']
        message.pose.position.y = pose['y']
        message.pose.orientation.x = qx
        message.pose.orientation.y = qy
        message.pose.orientation.z = qz
        message.pose.orientation.w = qw
        self.truth_publisher.publish(message)


def main() -> None:
    rclpy.init()
    bridge = Bridge()
    try:
        bridge.run()
    except KeyboardInterrupt:
        pass
    finally:
        bridge.destroy_node()
        rclpy.try_shutdown()


if __name__ == '__main__':
    main()

"""A ROS node that drives the robot to a goal, on simulation time.

Give it a goal with RViz's "2D Goal Pose" tool, or by publishing a
geometry_msgs/PoseStamped on /goal_pose in the odom frame. It steers by the
robot's odometry, as anything on the robot would have to, and stops short of
anything the lidar sees ahead.

It runs on simulation time (use_sim_time), so its 10 Hz is 10 Hz of simulated
time: headless, at many times real time, it decides many times a second of
wall-clock time, and the robot behaves exactly as it would in real time.

While it has a goal it owns the robot: its commands outrank the in-engine
autonomy. Once it arrives it stops sending, and control returns to whatever
the simulation would otherwise do.
"""

from __future__ import annotations

import math

import rclpy
from geometry_msgs.msg import PoseStamped, Twist
from nav_msgs.msg import Odometry
from rclpy.node import Node
from sensor_msgs.msg import LaserScan

from robodrive_bridge.steering import Scan, SteeringSettings, drive_towards


def yaw_from_quaternion(z: float, w: float) -> float:
    """Yaw of a rotation about +Z alone, which is all a ground robot has."""
    return 2.0 * math.atan2(z, w)


class GoalDriver(Node):

    def __init__(self) -> None:
        super().__init__('goal_driver')

        self.settings = SteeringSettings(
            max_speed_ms=self.declare_parameter('max_speed', 0.5).value,
            max_turn_rate_rads=self.declare_parameter('max_turn_rate', 1.0).value,
            goal_tolerance_m=self.declare_parameter('goal_tolerance', 0.3).value,
            stop_distance_m=self.declare_parameter('stop_distance', 0.6).value,
        )
        self.odom_frame = self.declare_parameter('odom_frame', 'odom').value
        rate_hz = self.declare_parameter('rate_hz', 10.0).value

        self.pose = None
        self.scan = None
        self.goal = None

        self.create_subscription(Odometry, 'odom', self.on_odometry, 10)
        self.create_subscription(LaserScan, 'scan', self.on_scan, 10)
        self.create_subscription(PoseStamped, 'goal_pose', self.on_goal, 10)
        self.command_publisher = self.create_publisher(Twist, 'cmd_vel', 10)

        # On the node's clock, which is simulation time when use_sim_time is
        # set: the timer does not fire until /clock arrives, and then fires
        # every tenth of a simulated second.
        self.create_timer(1.0 / rate_hz, self.on_timer)

    def on_odometry(self, message: Odometry) -> None:
        orientation = message.pose.pose.orientation
        self.pose = (message.pose.pose.position.x, message.pose.pose.position.y,
                     yaw_from_quaternion(orientation.z, orientation.w))

    def on_scan(self, message: LaserScan) -> None:
        self.scan = Scan(message.angle_min, message.angle_increment, list(message.ranges))

    def on_goal(self, message: PoseStamped) -> None:
        # Odometry is the only pose the robot has, so a goal must be in its
        # frame. Transforming from others would need a map, which there is not.
        if message.header.frame_id != self.odom_frame:
            self.get_logger().warning(
                f"Ignoring a goal in '{message.header.frame_id}': goals must be in '{self.odom_frame}'.")
            return

        self.goal = (message.pose.position.x, message.pose.position.y)
        self.get_logger().info(f'Driving to ({self.goal[0]:.2f}, {self.goal[1]:.2f})')

    def on_timer(self) -> None:
        if self.goal is None or self.pose is None:
            return

        command = drive_towards(*self.pose, *self.goal, self.scan, self.settings)

        twist = Twist()
        if command is None:
            self.get_logger().info('Arrived, by odometry')
            self.goal = None
        else:
            twist.linear.x, twist.angular.z = command
        self.command_publisher.publish(twist)


def main() -> None:
    rclpy.init()
    driver = GoalDriver()
    try:
        rclpy.spin(driver)
    except KeyboardInterrupt:
        pass
    finally:
        driver.destroy_node()
        rclpy.try_shutdown()


if __name__ == '__main__':
    main()

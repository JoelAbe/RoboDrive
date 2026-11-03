"""The bridge, the goal driver and RViz, all on simulation time.

    ros2 launch robodrive_bridge robodrive.launch.py
    ros2 launch robodrive_bridge robodrive.launch.py driver:=false ground_truth:=false
"""

from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.conditions import IfCondition
from launch.substitutions import LaunchConfiguration, PathJoinSubstitution
from launch_ros.actions import Node
from launch_ros.substitutions import FindPackageShare


def generate_launch_description() -> LaunchDescription:
    port = LaunchConfiguration('port')
    ground_truth = LaunchConfiguration('ground_truth')

    return LaunchDescription([
        DeclareLaunchArgument('port', default_value='8765',
                              description="The simulation API's port: 8765 in the editor"),
        DeclareLaunchArgument('ground_truth', default_value='true',
                              description="Publish the robot's true pose, to see odometry drift. "
                                          'For display only: nothing driving the robot should read it.'),
        DeclareLaunchArgument('driver', default_value='true',
                              description='Start the goal driver'),
        DeclareLaunchArgument('rviz', default_value='true',
                              description='Start RViz'),

        # The bridge publishes /clock, so it alone runs on wall-clock time.
        Node(
            package='robodrive_bridge',
            executable='bridge',
            parameters=[{'port': port, 'publish_ground_truth': ground_truth}],
            output='screen',
        ),
        Node(
            package='robodrive_bridge',
            executable='goal_driver',
            parameters=[{'use_sim_time': True}],
            output='screen',
            condition=IfCondition(LaunchConfiguration('driver')),
        ),
        Node(
            package='rviz2',
            executable='rviz2',
            arguments=['-d', PathJoinSubstitution([FindPackageShare('robodrive_bridge'), 'rviz', 'robodrive.rviz'])],
            parameters=[{'use_sim_time': True}],
            condition=IfCondition(LaunchConfiguration('rviz')),
        ),
    ])

from glob import glob

from setuptools import find_packages, setup

package_name = 'robodrive_bridge'

setup(
    name=package_name,
    version='0.1.0',
    packages=find_packages(exclude=['test']),
    data_files=[
        ('share/ament_index/resource_index/packages', ['resource/' + package_name]),
        ('share/' + package_name, ['package.xml']),
        ('share/' + package_name + '/launch', glob('launch/*.launch.py')),
        ('share/' + package_name + '/rviz', glob('rviz/*.rviz')),
    ],
    install_requires=['setuptools'],
    zip_safe=True,
    maintainer='Joel Abraham Orethu',
    maintainer_email='joelorethu@gmail.com',
    description='Bridges the RoboDrive Unreal simulation to ROS 2.',
    license='MIT',
    tests_require=['pytest'],
    entry_points={
        'console_scripts': [
            'bridge = robodrive_bridge.bridge:main',
            'goal_driver = robodrive_bridge.goal_driver:main',
        ],
    },
)

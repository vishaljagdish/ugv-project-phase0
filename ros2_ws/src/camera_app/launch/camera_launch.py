from launch import LaunchDescription
from launch_ros.actions import Node

def generate_launch_description():
    return LaunchDescription([
        Node(
            package='camera_app',
            executable='camera_node',
            parameters=[{
                'width': 1280,
                'height': 720,
                'framerate': 30,
                }]
            )
        ])

from launch import LaunchDescription
from launch_ros.actions import Node
from launch.actions import DeclareLaunchArgument, OpaqueFunction
from launch.substitutions import LaunchConfiguration

def launch_setup(context, *args, **kwargs):
    print("Start launch_setup");

    width = LaunchConfiguration("width");
    height = LaunchConfiguration("height");
    framerate = LaunchConfiguration("framerate");

    camera_node = Node(
            package='camera_app',
            executable='camera_node',
            parameters=[{      
                    "width": width,
                    "height": height,
                    "framerate": framerate,
                }],
            )

    nodes_to_start = [camera_node]

    return nodes_to_start

def generate_launch_description():
    declared_arguments = []

    declared_arguments.append(
            DeclareLaunchArgument(
                "width",
                default_value="800",
                description="",
            )
        )

    declared_arguments.append(
            DeclareLaunchArgument(
                "height",
                default_value="600",
                description="",
            )
        )

    declared_arguments.append(
            DeclareLaunchArgument(
                "framerate",
                default_value="30",
                description="",
            )
        )

    return LaunchDescription(declared_arguments + [OpaqueFunction(function=launch_setup)])

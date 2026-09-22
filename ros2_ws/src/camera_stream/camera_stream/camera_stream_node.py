import rclpy
from rclpy.node import Node
import subprocess

class CameraStreamerNode(Node):

    def __init__(self):
        super().__init__('camera_streamer')
        self.process = subprocess.Popen([
            "gst-launch-1.0",
            "libcamerasrc", 
            "!",
            "video/x-raw,colorimetry=bt709,format=NV12,width=800,height=600,framerate=30/1",
            "!",
            "queue",
            "!",
            "jpegenc",
            "!",
            "multipartmux",
            "!",
            "tcpserversink",
            "host=0.0.0.0",
            "port=5000"
            ])

    def stop(self):
        self.process.terminate()


def main(args=None):
    rclpy.init(args=args)

    camera_streamer = CameraStreamerNode()

    try:
        rclpy.spin(camera_streamer)
    except KeyboardInterrupt:
        pass
    finally:
        camera_streamer.stop()
        camera_streamer.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    main()

#GRPC Adapter Node
#Author:Vishal Jagdish

import rclpy
from rclpy.node import Node
from std_srvs.srv import Trigger
from rover_msgs.srv import Capture
from concurrent import futures
from threading import Event
import logging

import grpc
from grpc_bridge import rover_pb2, rover_pb2_grpc

class BridgeNode(Node):
    def __init__(self):
        super().__init__('grpc_bridge_node');
        self.toggle_client = self.create_client(Trigger, 'toggle_stream')
        self.capture_client = self.create_client(Capture, 'capture_snapshot')

    def call_service(self, client, request, timeout): #helper: calls request asynchronously
        if not client.service_is_ready():
            return None, "UNAVAILABLE"
        future = client.call_async(request)
        done = Event(); future.add_done_callback(lambda _: done.set())
        if not done.wait(timeout):
            client.remove_pending_request(future)
            return None, "TIMEOUT"
        return future.result(), None

class ToggleStream(rover_pb2_grpc.ToggleStreamServicer):
    def __init__(self, node): #reference to Node
        self.node = node

    def Toggle(self, request, context):
        print("processing toggle request")
        resp, err = self.node.call_service(self.node.toggle_client, Trigger.Request(), 1) 
        if err == "UNAVAILABLE": context.abort(UNAVAILABLE, "camera toggle service not up")
        if err == "TIMEOUT": context.abort(DEADLINE_UP, "camera did not answer")
        return rover_pb2.ToggleResponse(success = resp.success, message = resp.message)

class Snapshot(rover_pb2_grpc.SnapshotServicer):
    def __init__(self, node):
        self.node = node

    def Capture(self, request, context):
        print("processing capture request")
        resp, err = self.node.call_service(self.node.capture_client, Capture.Request(), 1)
        return rover_pb2.CaptureResponse(success = resp.success, message=resp.message, format=resp.image.format, image_data=bytes(resp.image.data))

def main(args=None):
    rclpy.init(args=args)
    node = BridgeNode()
    server = grpc.server(futures.ThreadPoolExecutor(max_workers=10))
    rover_pb2_grpc.add_ToggleStreamServicer_to_server(ToggleStream(node), server)
    rover_pb2_grpc.add_SnapshotServicer_to_server(Snapshot(node), server)
    server.add_insecure_port("192.168.50.69:50051")
    server.start()
    print("server started, listening on 50051")
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        server.stop(grace=1).wait()
        node.destroy_node()
        rclpy.shutdown()

if __name__ == '__main__':
    main()

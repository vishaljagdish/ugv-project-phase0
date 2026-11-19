import rclpy
from std_srvs.srv import Trigger
from concurrent import futures
import logging

import grpc
from grpc_bridge import rover_pb2, rover_pb2_grpc

class ToggleStream(rover_pb2_grpc.ToggleStreamServicer):
    def Toggle(self, request, context):
        print("processing toggle request")
        return rover_pb2.ToggleResponse(success = True, message="toggled stream")

class Snapshot(rover_pb2_grpc.SnapshotServicer):
    def Capure(self, request, context):
        print("processing captture request")
        return rover_pb2.CaptureResponse(success = True, message="captured snapshot")

def main():
    print('Hi from grpc_bridge.')
    port = "50051"
    server = grpc.server(futures.ThreadPoolExecutor(max_workers=10))
    rover_pb2_grpc.add_ToggleStreamServicer_to_server(ToggleStream(), server)
    rover_pb2_grpc.add_SnapshotServicer_to_server(Snapshot(), server)
    server.add_insecure_port("192.168.50.69:" + port)
    server.start()
    print("server started, listening on: " + port)
    server.wait_for_termination()

if __name__ == '__main__':
    logging.basicConfig()
    main()

minimal fpv cam app for rover, stream over udp + capture snapshots using gstreamer

to build:

gcc camera_stream.c -o camera_stream $(pkg-config --cflags --libs gstreamer-1.0 libgpiod)

//TODO
1. implement snapshot capture

2. replace placeholder gpio poll thread with external signal poller (RPC or ros service)

3. wrap around minimal ros node

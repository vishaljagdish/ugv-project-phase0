minimal cam app for rover, stream over udp + capture snapshots using gstreamer

to build:

gcc camera_stream.c -o camera_stream $(pkg-config --cflags --libs gstreamer-1.0 libgpiod)

//TODO

replace placeholder gpio poll thread with external signal poller (RPC or ros service)

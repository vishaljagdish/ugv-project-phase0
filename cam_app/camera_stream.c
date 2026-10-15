//Minimal Camera Livestreaming app for rover, pure C app
//Author: Vishal Jagdish

#include <gst/gst.h>
#include <stdbool.h>
#include <stdio.h>
#include <gpiod.h>
#include <pthread.h>

#define TOGGLE_STREAM_LINE 16
#define REQ_SNAPSHOT_LINE 21

//initialize global variables
struct gpiod_line_request *gpio_req = NULL;
bool enabled = true;
GstBus *bus = NULL;
char filename[100];
FILE *fp;

//custom data structure to pass values to functions
typedef struct {
	GstElement *pipeline;
	GstBus *bus;
	GstMessage *app_msg;
	struct gpiod_edge_event_buffer *edge_event_buffer;
	struct gpiod_line_request *request;
} CustomData;

//function to write JPEG encoded bytes to file
int handle_snapshot_request(GstBuffer *buffer)
{
	if (buffer == NULL) {
		g_printerr("cannot capture screenshot, image data unavailable!\n");
		return -1;
	}

	time_t now = time(NULL);
	struct tm *t = localtime(&now);
	GstMapInfo map_info;

	int cx = snprintf(filename, 100, "images/capture_%d_%d_%d_%dH%dM%dS.jpeg", t->tm_year + 1900, t->tm_mon + 1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);

	if (cx > 100) {
		g_print("warning: file output truncated.\n");
	}

	gst_buffer_map(buffer, &map_info, GST_MAP_READ); //this is how we access the actual image data
	fp = fopen(filename, "wb");
	if (fp == NULL) {
		g_printerr("could not open file, aborting...\n");
		return -1;
	}
	fwrite(map_info.data, map_info.size, 1, fp);
	fclose(fp);
	g_print("snapshot capture success. file size %zu bytes.\n", map_info.size);
	gst_buffer_unmap(buffer, &map_info);

	return 0;
}

void *gpio_thread(void *arg) //gpio edge event handling is done in a separate thread
{

	g_print("thread started.\n");

	CustomData *data = (CustomData *)arg; //this line tells the function to treat arg as a pointer to a CustomData object
	data->edge_event_buffer = gpiod_edge_event_buffer_new(16);

	while (1) { //this loop handles falling edge events (input pullup enabled)
	int ret = gpiod_line_request_read_edge_events(data->request, data->edge_event_buffer, 16);
	if (ret > 0) {
		bus = gst_pipeline_get_bus((GstPipeline *)data->pipeline);
		for (int i = 0; i < ret; ++i) {
			struct gpiod_edge_event *event = gpiod_edge_event_buffer_get_event(data->edge_event_buffer, i);
			int type = gpiod_edge_event_get_event_type(event);
			unsigned int offset = gpiod_edge_event_get_line_offset(event);

			if (type == GPIOD_EDGE_EVENT_FALLING_EDGE) {
				g_print("button pressed, pulling line %d low. \n", offset);
				switch (offset) {
					case TOGGLE_STREAM_LINE:
						GstStructure *toggle_struct = gst_structure_new_empty("toggle_stream");
						data->app_msg = gst_message_new_application(GST_OBJECT(data->pipeline), toggle_struct);
						g_print("toggling stream, posting application message to bus.\n");
						gst_bus_post(bus, data->app_msg);
						gst_object_unref(bus);
						break;
					case REQ_SNAPSHOT_LINE:
						GstStructure *snapshot_struct = gst_structure_new_empty("request_snapshot");
						data->app_msg = gst_message_new_application(GST_OBJECT(data->pipeline), snapshot_struct);
						g_print("requesting snapshot, posting application message to bus.\n");
						gst_bus_post(bus, data->app_msg);
						gst_object_unref(bus);
						break;
					default:
						break;
				}
			}
		}
	}
	}
}


int main(int argc, char *argv[])
{
    CustomData data;
    memset(&data, 0, sizeof(data));

    gst_init(&argc, &argv);

    struct gpiod_chip *chip = gpiod_chip_open("/dev/gpiochip4");
    struct gpiod_line_settings *settings = gpiod_line_settings_new();
    struct gpiod_line_config *line_cfg = gpiod_line_config_new();
    struct gpiod_request_config *req_cfg = gpiod_request_config_new();
    struct gpiod_edge_event_buffer *buffer = gpiod_edge_event_buffer_new(16);

    unsigned int in_offset[2] = {TOGGLE_STREAM_LINE, REQ_SNAPSHOT_LINE};
    gpiod_line_settings_set_direction(settings, GPIOD_LINE_DIRECTION_INPUT);
    gpiod_line_settings_set_bias(settings, GPIOD_LINE_BIAS_PULL_UP);
    gpiod_line_settings_set_edge_detection(settings, GPIOD_LINE_EDGE_FALLING);
    gpiod_line_settings_set_debounce_period_us(settings, 20000);
    gpiod_line_config_add_line_settings(line_cfg, in_offset, 2, settings);

    data.request = gpiod_chip_request_lines(chip, req_cfg, line_cfg);

    pthread_t thread1;

    data.pipeline = gst_pipeline_new("cam");
    GstElement *src = gst_element_factory_make("libcamerasrc", NULL);
    GstElement *tee = gst_element_factory_make("tee", NULL);
    GstElement *stream_filt = gst_element_factory_make("capsfilter", NULL);
    GstElement *stream_q = gst_element_factory_make("queue", NULL);
    GstElement *snapshot_q = gst_element_factory_make("queue", NULL);
    GstElement *stream_enc = gst_element_factory_make("x264enc", NULL);
    GstElement *snapshot_enc = gst_element_factory_make("jpegenc", NULL);
    GstElement *payloader = gst_element_factory_make("rtph264pay", NULL);
    GstElement *stream_sink  = gst_element_factory_make("udpsink", NULL);
    GstElement *snapshot_sink = gst_element_factory_make("appsink", NULL);
    GstElement *convert = gst_element_factory_make("videoconvert", NULL);
    GstMessage *msg;
    GstPad *tee_stream_pad, *tee_snapshot_pad;
    GstPad *stream_q_pad, *snapshot_q_pad;


    if (!data.pipeline || !src || !tee || !stream_filt || !convert || !stream_q || !snapshot_q || !stream_enc || !snapshot_enc || !payloader || !stream_sink || !snapshot_sink) {
	    g_printerr("Not all elements could be created.\n");
	    return -1;
    }

    //stream resolution, framerate and format are set through a capabilities filter
    GstCaps *stream_caps = gst_caps_from_string(
        "video/x-raw,format=NV12,width=800,height=600,framerate=30/1");

    g_object_set(stream_filt, "caps", stream_caps, NULL);

    gst_caps_unref(stream_caps);

    g_object_set(stream_enc,  "tune", 0x00000004, "speed-preset", 1, NULL);
    g_object_set(stream_sink, "host", "192.168.50.10", "port", 5000, "sync", FALSE, NULL);
    g_object_set(snapshot_enc, "snapshot", false, NULL); //snapshot = true triggers EOS which we dont want
    g_object_set(snapshot_sink, "max-buffers", 0, "emit-signals", TRUE,  "drop", TRUE, NULL); //max-buffers = 0 means unlimited buffers

    gst_bin_add_many(GST_BIN(data.pipeline), src, stream_filt, tee, stream_q, stream_enc, payloader, stream_sink, snapshot_q, convert, snapshot_enc, snapshot_sink, NULL);
    if (!gst_element_link_many(src, stream_filt, tee, NULL) || !gst_element_link_many(stream_q, stream_enc, payloader, stream_sink, NULL) 
		   || !gst_element_link_many(snapshot_q, convert, snapshot_enc, snapshot_sink, NULL)) {
        g_printerr("link failed\n");
	gst_object_unref(data.pipeline);
        return 1;
    }

    //tees contain request pads which require manual linking
    tee_stream_pad = gst_element_request_pad_simple(tee, "src_%u");
    g_print("Obtained request pad %s for stream branch.\n", gst_pad_get_name(tee_stream_pad));
    stream_q_pad = gst_element_get_static_pad(stream_q, "sink");

    tee_snapshot_pad = gst_element_request_pad_simple(tee, "src_%u");
    g_print("Obtained request pad %s for snapshot branch.\n", gst_pad_get_name(tee_snapshot_pad));
    snapshot_q_pad = gst_element_get_static_pad(snapshot_q, "sink");

    if (gst_pad_link(tee_stream_pad, stream_q_pad) != GST_PAD_LINK_OK) {
	    g_printerr("could not link tee with stream queue.\n");
	    return -1;
    }

    if (gst_pad_link(tee_snapshot_pad, snapshot_q_pad) != GST_PAD_LINK_OK) {
	    g_printerr("Could not link tee with snapshot queue.\n");
	    return -1;
    }

    gst_object_unref(stream_q_pad);
    gst_object_unref(snapshot_q_pad);

    /* Block until an error or end-of-stream */
    bus = gst_element_get_bus(data.pipeline);

    //create gpio event handler thread
    pthread_create(&thread1, NULL, gpio_thread, &data);

	 do { 
		 msg = gst_bus_timed_pop_filtered(bus, 100 * GST_MSECOND,
                                                 GST_MESSAGE_ERROR | GST_MESSAGE_EOS | GST_MESSAGE_APPLICATION);


   	 if (msg != NULL) {
		 GError *err;
		 gchar *debug_info;

		 switch (GST_MESSAGE_TYPE(msg)) {
			 case GST_MESSAGE_ERROR:
				gst_message_parse_error(msg, &err, &debug_info);
				g_printerr("error recievied from element: %s: %s\n", GST_OBJECT_NAME(msg->src), err->message);
				g_printerr("debugging information: %s\n", debug_info ? debug_info : "none");
				g_clear_error(&err);
				g_free(debug_info);
				break;
			case GST_MESSAGE_EOS:
				g_print("end-of-stream-triggered by: %s\n", GST_OBJECT_NAME(msg->src));
				break;
			case GST_MESSAGE_APPLICATION: //main app logic
				g_print("received msg from application\n");
				GstState current_state; 			
				gst_element_get_state(GST_ELEMENT(data.pipeline), &current_state, NULL, 0);
				const gchar *state_name = gst_element_state_get_name(current_state);
				const GstStructure *s;
				s = gst_message_get_structure(data.app_msg);
				g_print("received message: %s\n", gst_structure_get_name(s));
				if (gst_structure_has_name(s, "toggle_stream")) {
					if (current_state != GST_STATE_PLAYING) {
						gst_element_set_state(data.pipeline, GST_STATE_PLAYING);
						g_print("setting current state to PLAYING.\n");
					} else { 
						gst_element_set_state(data.pipeline, GST_STATE_PAUSED);
						g_print("setting current state to PAUSED.\n");
					}
				} else {
					if (current_state != GST_STATE_PLAYING) {
						g_print("cannot capture screenshot, livestream not running!\n");
					} else {
						g_print("capturing snapshot...\n");
						GstSample *sample = NULL;
						GstBuffer *buf;
						g_object_get(snapshot_sink, "last-sample", &sample, NULL);

						if (sample) {
							buf = gst_sample_get_buffer(sample);
							handle_snapshot_request(buf);
							gst_sample_unref(sample);
						}

					}
				}
				gst_message_unref(data.app_msg);
				break;
			default: //default case should never be reached
				g_print("Message Type: %d\n", GST_MESSAGE_TYPE(msg));
				break;
		 }
	 }
	 } while (enabled);
				
    gst_message_unref(msg);
    gst_object_unref(bus);
    gst_element_release_request_pad(tee, tee_stream_pad);
    gst_element_release_request_pad(tee, tee_snapshot_pad);
    gst_object_unref(tee_stream_pad);
    gst_object_unref(tee_snapshot_pad);
    gst_element_set_state(data.pipeline, GST_STATE_NULL);
    gst_object_unref(data.pipeline);
    gpiod_edge_event_buffer_free(data.edge_event_buffer);
    gpiod_line_request_release(gpio_req);
    return 0;
}

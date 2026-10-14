#include <gst/gst.h>
#include <stdbool.h>
#include <stdio.h>
#include <gpiod.h>
#include <pthread.h>

#define TOGGLE_STREAM_LINE 16
#define REQ_SNAPSHOT_LINE 21

struct gpiod_line_request *gpio_req = NULL;
bool enabled = true;
GstBus *bus = NULL;
const char* filename = "test.jpeg";
FILE *fp;

typedef struct {
	GstElement *pipeline;
	GstBus *bus;
	GstMessage *app_msg;
	struct gpiod_line_request *request;
} CustomData;

static GstFlowReturn new_sample(GstElement *sink, void *data)
{
	GstSample *sample;
	GstBuffer *buffer;

	g_signal_emit_by_name(sink, "pull-sample", &sample);
	if (sample) {
		g_print("size: %zu\n", gst_buffer_get_size(gst_sample_get_buffer(sample)));
		gst_sample_unref(sample);
		return GST_FLOW_OK;
	}

	return GST_FLOW_FLUSHING;
}

void *gpio_thread(void *arg) 
{

	g_print("thread started.\n");

	CustomData *data = (CustomData *)arg;
	struct gpiod_edge_event_buffer *buffer = gpiod_edge_event_buffer_new(16);

	while (1) {
	int ret = gpiod_line_request_read_edge_events(data->request, buffer, 16);
	if (ret > 0) {
		//message = gst_message_new_application(GST_OBJECT(data->pipeline), structure);
		bus = gst_pipeline_get_bus((GstPipeline *)data->pipeline);
		for (int i = 0; i < ret; ++i) {
			struct gpiod_edge_event *event = gpiod_edge_event_buffer_get_event(buffer, i);
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
    GstElement *snapshot_filt = gst_element_factory_make("capsfilter", NULL);
    GstElement *stream_q = gst_element_factory_make("queue", NULL);
    GstElement *snapshot_q = gst_element_factory_make("queue", NULL);
    GstElement *stream_enc = gst_element_factory_make("x264enc", NULL);
    GstElement *snapshot_enc = gst_element_factory_make("jpegenc", NULL);
    GstElement *payloader = gst_element_factory_make("rtph264pay", NULL);
    GstElement *stream_sink  = gst_element_factory_make("udpsink", NULL);
    GstElement *snapshot_sink = gst_element_factory_make("appsink", NULL);
    GstElement *convert = gst_element_factory_make("videoconvert", NULL);
    GstMessage *msg;
    //GstBus *bus;
    GstPad *tee_stream_pad, *tee_snapshot_pad;
    GstPad *stream_q_pad, *snapshot_q_pad;

    //data.toggle_structure = gst_structure_new_empty("toggle_stream");
    //data.snapshot_structure = gst_structure_new_empty("request_snapshot");

    if (!data.pipeline || !src || !tee || !stream_filt || !snapshot_filt || !convert || !stream_q || !snapshot_q || !stream_enc || !snapshot_enc || !payloader || !stream_sink || !snapshot_sink) {
	    g_printerr("Not all elements could be created.\n");
	    return -1;
    }

    GstCaps *stream_caps = gst_caps_from_string(
        "video/x-raw,format=NV12,width=800,height=600,framerate=30/1");
    GstCaps *snapshot_caps = gst_caps_from_string(
		   "video/x-raw,format=RGB,width=800,height=600");

    g_object_set(stream_filt, "caps", stream_caps, NULL);
    g_object_set(snapshot_filt, "caps", snapshot_caps, NULL);

    gst_caps_unref(stream_caps);
    gst_caps_unref(snapshot_caps);

    g_object_set(stream_enc,  "tune", 0x00000004, "speed-preset", 1, NULL);
    g_object_set(stream_sink, "host", "192.168.50.10", "port", 5000, "sync", FALSE, NULL);
    g_object_set(snapshot_enc, "snapshot", false, NULL);
    //g_object_set(snapshot_sink, "location", "snapshot_test.png", NULL);
    g_object_set(snapshot_sink, "max-buffers", 0, "emit-signals", TRUE, NULL);
    //g_signal_connect(snapshot_sink, "new-sample", G_CALLBACK(new_sample), NULL);

    gst_bin_add_many(GST_BIN(data.pipeline), src, stream_filt, tee, stream_q, stream_enc, payloader, stream_sink, snapshot_q, convert, snapshot_enc, snapshot_sink, NULL);
    if (!gst_element_link_many(src, stream_filt, tee, NULL) || !gst_element_link_many(stream_q, stream_enc, payloader, stream_sink, NULL) 
		   || !gst_element_link_many(snapshot_q, convert, snapshot_enc, snapshot_sink, NULL)) {
        g_printerr("link failed\n");
	gst_object_unref(data.pipeline);
        return 1;
    }

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

    //gst_element_set_state(data.pipeline, GST_STATE_PLAYING);
    /* Block until an error or end-of-stream */
    bus = gst_element_get_bus(data.pipeline);
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
				//gst_element_set_state(pipeline, GST_STATE_PLAYING);
				break;
			case GST_MESSAGE_APPLICATION:
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
						GstSample *sample;
						GstBuffer *buf;
						g_signal_emit_by_name(snapshot_sink, "pull-sample", &sample);

						if (sample) {
							buf = gst_sample_get_buffer(sample);
							g_print("size of buffer: %zu bytes.\n", gst_buffer_get_size(buf));
							GstMapInfo map_info;
							gst_buffer_map(buf, &map_info, GST_MAP_READ);
							fp = fopen("test.jpeg", "wb");
							if (fp == NULL) {
								g_printerr("Error opening file.");
							}
							size_t written = fwrite(map_info.data, map_info.size, 1, fp);
							g_print("captured screenshot, file size: %zu bytes.\n", map_info.size);
							fclose(fp);
							gst_buffer_unmap(buf, &map_info);
							gst_sample_unref(sample);
						}

					}
				}
				gst_message_unref(data.app_msg);
				break;
			default:
				g_print("Message Type: %d\n", GST_MESSAGE_TYPE(msg));
				break;
		 }
		 //gst_element_get_state(GST_ELEMENT(data.pipeline), &current_state, NULL, 0);
		 //gst_element_set_state(pipeline, GST_STATE_PLAYING);
		 //gst_message_unref(data.toggle_msg);
	 }
	 } while (enabled);
				
    //gst_object_unref(data.toggle_message);
    //gst_object_unref(data.snapshot_message);
    gst_object_unref(bus);
    gst_element_release_request_pad(tee, tee_stream_pad);
    gst_element_release_request_pad(tee, tee_snapshot_pad);
    gst_object_unref(tee_stream_pad);
    gst_object_unref(tee_snapshot_pad);
    gst_element_set_state(data.pipeline, GST_STATE_NULL);
    gst_object_unref(data.pipeline);
   
    gpiod_line_request_release(gpio_req);
    return 0;
}

static void handle_request(GstElement *pipeline);

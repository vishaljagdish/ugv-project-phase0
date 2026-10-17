//Camera Node. Available Services: 1) Enable/Disable Streaming 2) Capture Frame
//Author: Vishal Jagdish

#include "rclcpp/rclcpp.hpp"
#include <gst/gst.h>
#include "std_srvs/srv/trigger.hpp"

#include <cstdio>
#include <ctime>

using std::placeholders::_1;
using std::placeholders::_2;

int width = 800;
int height = 600;
int framerate = 30;
char filter_params[64];
char filename[100];
FILE *fp;
static const rclcpp::Logger LOGGER = rclcpp::get_logger("camera_node");

bool handle_capture_request(GstBuffer *buffer)
{
	if (buffer == nullptr) {
		RCLCPP_ERROR(LOGGER, "cannot capture screenshot, image data unavailable.\n");
		return false;
	}
	
	time_t now = time(NULL);
	struct tm *timestamp = localtime(&now);
	GstMapInfo map_info;

	int cx = std::snprintf(filename, sizeof(filename), "images/capture_%d_%d_%d_%dH%dM%dS.jpeg", timestamp->tm_year + 1900, timestamp->tm_mon + 1,
			timestamp->tm_mday, timestamp->tm_hour, timestamp->tm_min, timestamp->tm_sec);

	if (cx > 100) RCLCPP_WARN(LOGGER, "warning: file output truncated.\n");

	gst_buffer_map(buffer, &map_info, GST_MAP_READ);
	fp = std::fopen(filename, "wb");
	if (fp == nullptr) {
	       RCLCPP_ERROR(LOGGER, "could not open file.\n");
		return false;
	}
	std::fwrite(map_info.data, map_info.size, 1, fp);
	std::fclose(fp);
	gst_buffer_unmap(buffer, &map_info);

	RCLCPP_INFO(LOGGER,"saved capture as %s", filename);

	return true;	
}

class CameraNode : public rclcpp::Node
{
	public:
		CameraNode() : Node("camera_node") {
			this->declare_parameter("width", width);
			this->declare_parameter("height", height);
			this->declare_parameter("framerate", framerate);

			pipeline = gst_pipeline_new("camera_pipeline");
			stream_src = gst_element_factory_make("libcamerasrc", nullptr);
			tee = gst_element_factory_make("tee", nullptr);
			filter = gst_element_factory_make("capsfilter", nullptr);
			stream_q = gst_element_factory_make("queue", nullptr);
			stream_enc = gst_element_factory_make("x264enc", nullptr);
			payloader = gst_element_factory_make("rtph264pay", nullptr);
			stream_sink = gst_element_factory_make("udpsink", nullptr);
			capture_q = gst_element_factory_make("queue", nullptr);
			capture_enc = gst_element_factory_make("jpegenc", nullptr);
			capture_sink = gst_element_factory_make("appsink", nullptr);
			convert = gst_element_factory_make("videoconvert", nullptr);


			if (!pipeline || !stream_src || !tee || !filter || !stream_q || !stream_enc || !payloader || !stream_sink || !capture_q || !capture_enc || !convert || !capture_sink ) {
				throw std::runtime_error("not all elements could be created.\n");
			}

			std::snprintf(filter_params, sizeof(filter_params), "video/x-raw,format=NV12,width=%ld,height=%ld,framerate=%ld/1", 
					this->get_parameter("width").as_int(), this->get_parameter("height").as_int(), this->get_parameter("framerate").as_int());
			GstCaps *stream_caps = gst_caps_from_string(filter_params);
			g_object_set(filter, "caps", stream_caps, nullptr);
			gst_caps_unref(stream_caps);

			g_object_set(stream_enc, "tune", 0x00000004, "speed-preset", 1, nullptr);
			g_object_set(stream_sink, "host", "192.168.50.10", "port", 5000, "sync", false, nullptr);
			g_object_set(capture_enc, "snapshot", false, nullptr);
			g_object_set(capture_sink, "max-buffers", 0, "emit-signals", true, "drop", true, nullptr);

			gst_bin_add_many(GST_BIN(pipeline), stream_src, filter, tee, stream_q, stream_enc, payloader, stream_sink, capture_q, convert, capture_enc, capture_sink, nullptr);
			if (!gst_element_link_many(stream_src, filter, tee, nullptr) || !gst_element_link_many(stream_q, stream_enc, payloader, stream_sink, nullptr)
					|| !gst_element_link_many(capture_q, convert, capture_enc, capture_sink, nullptr)) {
			       throw std::runtime_error("could not link elements.\n");
				gst_object_unref(pipeline);
			}


			tee_stream_pad = gst_element_request_pad_simple(tee, "src_%u");
			RCLCPP_INFO(LOGGER, "Obtained request pad %s for stream branch.\n", gst_pad_get_name(tee_stream_pad));
			GstPad *stream_q_pad = gst_element_get_static_pad(stream_q, "sink");

			tee_capture_pad = gst_element_request_pad_simple(tee, "src_%u");
			RCLCPP_INFO(LOGGER, "Obtained request pad %s for capture branch.\n", gst_pad_get_name(tee_capture_pad));
			GstPad *capture_q_pad = gst_element_get_static_pad(capture_q, "sink");

			if (gst_pad_link(tee_stream_pad, stream_q_pad) != GST_PAD_LINK_OK) {
				throw std::runtime_error("could not link tee with stream queue.\n");
			}

			if (gst_pad_link(tee_capture_pad, capture_q_pad) != GST_PAD_LINK_OK) {
				throw std::runtime_error("could not link tee with capture queue");
			}

			gst_object_unref(stream_q_pad);
			gst_object_unref(capture_q_pad);

			gst_element_set_state(pipeline, GST_STATE_READY);

			toggle_service = create_service<std_srvs::srv::Trigger>("toggle_stream", std::bind(&CameraNode::toggle_service_cb, this, _1, _2));
			RCLCPP_INFO(LOGGER, "created toggle service.\n");
			snapshot_service = create_service<std_srvs::srv::Trigger>("capture", std::bind(&CameraNode::snapshot_service_cb, this, _1, _2));
			RCLCPP_INFO(LOGGER, "created capture service.\n");		
		}

		~CameraNode() {
			gst_element_release_request_pad(tee, tee_stream_pad);
			gst_element_release_request_pad(tee, tee_capture_pad);
			gst_object_unref(tee_stream_pad);
			gst_object_unref(tee_capture_pad);
			gst_element_set_state(pipeline, GST_STATE_NULL);
			gst_object_unref(pipeline);
		}

	private: 
		void toggle_service_cb(const std::shared_ptr<std_srvs::srv::Trigger::Request> request, 
				std::shared_ptr<std_srvs::srv::Trigger::Response> response) 
		{	
			gst_element_get_state(pipeline, &pipeline_state, nullptr, 0);

			if (pipeline_state != GST_STATE_NULL) {
				if (pipeline_state != GST_STATE_PLAYING) {
					gst_element_set_state(pipeline, GST_STATE_PLAYING);
					RCLCPP_INFO(LOGGER, "set pipeline state to PLAYING.\n");
				} else {
					gst_element_set_state(pipeline, GST_STATE_PAUSED);
					RCLCPP_INFO(LOGGER, "set pipeline state to PAUSED.\n");
				}
				response->success = true;
				response->message = "toggle success";
			} else {
				RCLCPP_ERROR(LOGGER, "could not set pipeline state.\n");
				response->success = false;
				response->message = "toggle failed";
			}

		}

		void snapshot_service_cb(const std::shared_ptr<std_srvs::srv::Trigger::Request> request,
				std::shared_ptr<std_srvs::srv::Trigger::Response> response)
		{
			gst_element_get_state(pipeline, &pipeline_state, nullptr, 0);

			if (pipeline_state == GST_STATE_PLAYING) {
				RCLCPP_INFO(LOGGER, "capturing frame...\n");
				g_object_get(capture_sink, "last_sample", &sample, NULL);
				if (sample) {
					buf = gst_sample_get_buffer(sample);
					if (!handle_capture_request(buf)) {
						response->success = false;
						response->message = "an error occurred while capturing";
					} else {
						response->success = true;
						response->message = "capture sucess";
					}
				}
			} else {
				RCLCPP_WARN(LOGGER, "cannot capture frame, stream is not running!\n");
				response->success = false;
				response->message = "cannot capture frame, stream is not running!";
			}

		}

		rclcpp::Service<std_srvs::srv::Trigger>::SharedPtr toggle_service;
		rclcpp::Service<std_srvs::srv::Trigger>::SharedPtr snapshot_service;
		GstElement *pipeline;
		GstElement *stream_src;
		GstElement *tee;
		GstElement *filter;
		GstElement *stream_q;
		GstElement *stream_enc;
		GstElement *payloader;
		GstElement *stream_sink;
		GstElement *capture_q;
		GstElement *capture_enc;
		GstElement *capture_sink;
		GstElement *convert;
		GstPad *tee_stream_pad, *tee_capture_pad;
		GstSample *sample;
		GstBuffer *buf;
		GstState pipeline_state;
};

int main(int argc, char **argv) 
{
	gst_init(&argc, &argv);
	rclcpp::init(argc, argv);
	rclcpp::spin(std::make_shared<CameraNode>());
	rclcpp::shutdown();
	return 0;
}

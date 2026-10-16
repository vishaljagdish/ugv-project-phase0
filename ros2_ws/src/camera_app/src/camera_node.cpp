#include "rclcpp/rclcpp.hpp"
#include <gst/gst.h>
#include "std_srvs/srv/trigger.hpp"

using std::placeholders::_1;
using std::placeholders::_2;

static const rclcpp::Logger LOGGER = rclcpp::get_logger("camera_node");

class CameraNode : public rclcpp::Node
{
	public:
		CameraNode() : Node("camera_node") {
			pipeline = gst_pipeline_new("camera_pipeline");
			stream_src = gst_element_factory_make("libcamerasrc", nullptr);
			tee = gst_element_factory_make("tee", nullptr);
			filter = gst_element_factory_make("capsfilter", nullptr);
			stream_q = gst_element_factory_make("queue", nullptr);
			stream_enc = gst_element_factory_make("x264enc", nullptr);
			payloader = gst_element_factory_make("rtph264pay", nullptr);
			sink = gst_element_factory_make("udpsink", nullptr);

			if (!pipeline || !stream_src || !tee || !filter || !stream_q || !stream_enc || !payloader || !sink ) {
				throw std::runtime_error("not all elements could be created.\n");
			}

			GstCaps *stream_caps = gst_caps_from_string("video/x-raw,format=NV12,width=800,height=600,framerate=30/1");
			g_object_set(filter, "caps", stream_caps, nullptr);
			gst_caps_unref(stream_caps);

			g_object_set(stream_enc, "tune", 0x00000004, "speed-preset", 1, nullptr);
			g_object_set(sink, "host", "192.168.50.10", "port", 5000, "sync", false, nullptr);

			gst_bin_add_many(GST_BIN(pipeline), stream_src, filter, stream_q, stream_enc, payloader, sink, NULL);
			if (!gst_element_link_many(stream_src, filter, stream_q, stream_enc, payloader, sink, NULL)) {
			       throw std::runtime_error("could not link elements.\n");
				gst_object_unref(pipeline);
			}

			gst_element_set_state(pipeline, GST_STATE_READY);

			toggle_service = create_service<std_srvs::srv::Trigger>("toggle_stream", std::bind(&CameraNode::toggle_service_cb, this, _1, _2));
			RCLCPP_INFO(LOGGER, "created toggle service.\n");		
		}

		~CameraNode() {
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

		rclcpp::Service<std_srvs::srv::Trigger>::SharedPtr toggle_service;
		GstElement *pipeline;
		GstElement *stream_src;
		GstElement *tee;
		GstElement *filter;
		GstElement *stream_q;
		GstElement *stream_enc;
		GstElement *payloader;
		GstElement *sink;
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

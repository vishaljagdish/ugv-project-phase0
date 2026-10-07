#ifndef CAMERA_NODE_HPP
#define CAMERA_NODE_HPP

#include "rclcpp/rclcpp.hpp"
#include <gst/gst.h>

class CameraNode : public rclcpp::Node
{
	public:
		CameraNode(const rclcpp::NodeOptions node_options);
	private:
		GstElement *pipeline;
		Gstelement *src;
		GstElement *filter;
		GstElement *q;
		GstElement *enc;
		GstElement *payloader;
		GstElement *sink;
		GstMessage *msg;
		GstBus *bus;
}


#endif

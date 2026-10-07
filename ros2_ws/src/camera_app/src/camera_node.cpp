#include <camera_node.hpp>

static constexpr rclcpp::Logger LOGGER = rclcpp::get_logger("camrea_node");

CameraNode::CameraNode(const rclcpp::NodeOptions node_options) : rclcpp::Node("camera_node", node_options) 
{
	pipeline = gst_pipeline_new("camera_pipeline");

}
int main(int argc, char ** argv)
{



  return 0;
}

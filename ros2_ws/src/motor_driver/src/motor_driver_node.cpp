#include "motor_driver.hpp"

static const rclcpp::Logger LOGGER = rclcpp::get_logger("motor_driver_node");

MotorDriverNode::MotorDriverNode() : Node("motor_driver_node") 
{
	RCLCPP_INFO(LOGGER, "Hello Motor Driver Node\n");
}

MotorDriverNode::~MotorDriverNode()
{
}

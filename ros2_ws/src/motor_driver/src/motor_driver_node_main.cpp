#include "motor_driver.hpp"

//implement node as rclcpp::LifecycleNode

int main(int argc, char ** argv)
{
	rclcpp::init(argc, argv);
	rclcpp::spin(std::make_shared<MotorDriverNode>());
	rclcpp::shutdown();
	return 0;
}

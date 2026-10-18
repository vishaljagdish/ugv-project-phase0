//Node to communicate with Motor Driver Board via U(S)ART. Sends speed/pwm commands, Publishes Encoder Data
//Author: Vishal Jagdish


#include "rclcpp/rclcpp.hpp"

#include <termios.h>

//implement node as rclcpp::LifecycleNode

class MotorDriverNode : public rclcpp::Node 
{
	public:
		MotorDriverNode() : Node("motor_driver_node") {}
		~MotorDriverNode() {}
	private:
		//create publisher, subscriber, define callbacks
		//cerate struct to store uart device info

};

int main(int argc, char ** argv)
{
	rclcpp::init(argc, argv);
	rclcpp::spin(std::make_shared<MotorDriverNode>());
	rclcpp::shutdown();
	return 0;
}

//Node to communicate with Motor Driver Board via U(S)ART. Sends speed/pwm commands, and publishs encoder data
////Author: Vishal Jagdish


#ifndef MOTOR_DRIVER_HPP
#define MOTOR_DRIVER_HPP

#include "rclcpp/rclcpp.hpp"
#include "std_srvs/srv/trigger.hpp"
#include "geometry_msgs/msg/twist_stamped.hpp"

class MotorDriverNode : public rclcpp::Node 
{
	public:
		MotorDriverNode();
		~MotorDriverNode();

	private:
		rclcpp::Subscription<geometry_msgs::msg::TwistStamped>::SharedPtr cmd_vel_sub;
};

#endif

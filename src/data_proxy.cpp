#include "messagebus.h"
#include "ros2_channel.h"
#include "mqtt_channel.h"
#include "logic/scheduler_logic.h"


int main2(int argc, char** argv)
{
	rclcpp::init(argc, argv, rclcpp::InitOptions(), rclcpp::SignalHandlerOptions::None);
	MessageBus<InternalMessage> message_bus;
	auto ros2 = std::make_shared<Ros2Channel>("test_node", &message_bus);
	ros2->start();
	auto mqtt = std::make_shared<MqttChannel>(&message_bus);
	mqtt->init("admin", "123456", "172.23.19.67");
	mqtt->start();
	CoExecutor co_executor(4);
	co_executor.start();


	SchedulerLogic scheduler_logic(&co_executor, ros2, mqtt);
	scheduler_logic.init();

	message_bus.run();
	return 0;
}

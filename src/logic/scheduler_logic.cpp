#include <std_msgs/msg/string.hpp>
#include "data_proxy/utils.h"

#include "scheduler_logic.h"




bool SchedulerLogic::init()
{
    ros2_ptr_->subscribe<std_msgs::msg::String>("ros2_test_topic");
    mqtt_ptr_->subscribe("platform/mqtt_test_topic");
    handle_ros2_test_topic();
    handle_mqtt_test_topic();
    return true;
}


CoTask SchedulerLogic::handle_ros2_test_topic()
{
    auto await = message_bus_->create_message_await(co_executor_, "ros2_test_topic");
    while (true) 
    {
        auto imsg = co_await await;
        std_msgs::msg::String msg;
        Ros2MessageParseFromString(imsg.data, msg);
        std::cout << "ros2_test_topic: " << msg.data << std::endl;

        mqtt_ptr_->publish("platform/ros22mqtt", msg.data);
    }
}

CoTask SchedulerLogic::handle_mqtt_test_topic()
{
    auto await = message_bus_->create_message_await(co_executor_, "platform/mqtt_test_topic");
    while (true) 
    {
        auto imsg = co_await await;
        std::cout << "mqtt_test_topic: " << imsg.data << std::endl;

        std_msgs::msg::String msg;
        msg.set__data(imsg.data);
        ros2_ptr_->publish("platform/mqtt2ros2", msg);
    }
}
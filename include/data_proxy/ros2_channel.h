#pragma once
#include <rclcpp/rclcpp.hpp>
#include <rclcpp/serialization.hpp>
#include <std_msgs/msg/string.hpp>

#include "messagebus.h"
#include "thread_task.h"

class Ros2Channel : public rclcpp::Node, public ThreadTask
{
   public:
    Ros2Channel(const string& name, MessageBus<InternalMessage>* message_bus,
                const rclcpp::NodeOptions& options = rclcpp::NodeOptions())
        : Node(name, options), message_bus_(message_bus)
    {
    }

    bool init() { return true; }

    virtual void run() override
    {
        rclcpp::spin(shared_from_this());  // block
        rclcpp::shutdown();
    }

    template <typename MessageType>
    bool publish(const string& topic, const MessageType& msg)
    {
        using PublishType = rclcpp::Publisher<MessageType, std::allocator<void>>;
        publishers_mutex_.lock();
        if (!publishers.contains(topic))
        {
            publishers[topic] = create_publisher<MessageType>(topic, rclcpp::QoS(10));
        }
        auto pub = std::static_pointer_cast<PublishType>(publishers[topic]);
        pub->publish(msg);
        publishers_mutex_.unlock();
        RCLCPP_INFO(get_logger(), fmt::format("publishing to topic: {}", topic).c_str());
        return true;
    }

    template <typename MessageType>
    bool subscribe(const string& topic)
    {
        // using SubscribeType = rclcpp::Subscription<MessageType, std::allocator<void>>;
        if (subscribers.contains(topic))
        {
            RCLCPP_WARN(get_logger(), fmt::format("already subscribe topic: {}", topic).c_str());
            return false;
        }
        subscribers[topic] = create_subscription<MessageType>(
            topic, rclcpp::QoS(10),
            [topic, self = std::dynamic_pointer_cast<Ros2Channel>(shared_from_this()),
             serializer = rclcpp::Serialization<MessageType>(),
             serialized_msg = rclcpp::SerializedMessage()](const MessageType& message)
            {
                serializer.serialize_message(&message, const_cast<rclcpp::SerializedMessage*>(&serialized_msg));
                InternalMessage imsg;
                imsg.name = topic;
                imsg.data.assign(reinterpret_cast<char*>(serialized_msg.get_rcl_serialized_message().buffer),
                                 serialized_msg.size());
                self->message_bus_->push_message(std::move(imsg));
            });
        return true;
    }

    MessageBus<InternalMessage>* get_message_bus() { return message_bus_; }

   private:
    unordered_map<string, shared_ptr<void>> publishers;
    std::mutex publishers_mutex_;
    unordered_map<string, shared_ptr<void>> subscribers;
    MessageBus<InternalMessage>* message_bus_;
};
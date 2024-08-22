#pragma once
#include <rclcpp/rclcpp.hpp>
#include <rclcpp/serialization.hpp>
#include "data_proxy/common.h"

template<typename T>
inline void Ros2MessageParseFromString(const string& str, T& msg)
{
    rclcpp::SerializedMessage serialized_msg;
    serialized_msg.reserve(str.size());
    std::memcpy(serialized_msg.get_rcl_serialized_message().buffer, str.data(), str.size());
    //serialized_msg.get_rcl_serialized_message().buffer = reinterpret_cast<uint8_t*>(const_cast<char*>(str.data()));
    serialized_msg.get_rcl_serialized_message().buffer_length = str.size();
    rclcpp::Serialization<T> serializer;
    serializer.deserialize_message(&serialized_msg, &msg);
}

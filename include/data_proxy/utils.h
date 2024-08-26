#pragma once
#include <rclcpp/rclcpp.hpp>
#include <rclcpp/serialization.hpp>
#include "data_proxy/common.h"
#include <type_traits>
#include <sstream>

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




//返回时间戳单位毫秒
inline std::time_t geTtimestamp()
{
    auto tp = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now());
    auto tmp = std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch());
    std::time_t timestamp = tmp.count();
    return timestamp;
}


template<typename I, std::enable_if_t<std::is_integral_v<I>, bool> = true>
I String2Int (const string& v)
{
    I i;
    std::stringstream ss(v);
    ss >> i;
    return i;
}

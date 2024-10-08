#pragma once

#include "data_proxy/common.h"
#include "messagebus.h"
#include "mqtt_channel.h"
#include "ros2_channel.h"
#include "timer_mgr.h"

class ILogic
{
   public:
    ILogic(CoExecutor* co_executor, TimerMgr* timer_mgr, shared_ptr<Ros2Channel> ros2_ptr,
           shared_ptr<MqttChannel> mqtt_ptr)
        : co_executor_(co_executor), timer_mgr_(timer_mgr), ros2_ptr_(ros2_ptr), mqtt_ptr_(mqtt_ptr)
    {
        message_bus_ = ros2_ptr_->get_message_bus();
    }
    virtual bool init() = 0;
    virtual ~ILogic() {}

   protected:
    SharedMessageAwait<InternalMessage> shared_message_await(const string& msg_name, uint8_t priority = 0)
    {
        return message_bus_->create_shared_message_await(co_executor_, msg_name, priority);
    }
    MessageAwait<InternalMessage> message_await(const string& msg_name, uint8_t priority = 0)
    {
        return message_bus_->create_message_await(co_executor_, msg_name, priority);
    }
    TempMessageAwait<InternalMessage> temp_message_await(
        const string& msg_name, uint8_t priority = 0,
        function<bool(const InternalMessage& msg)>&& filter = [](const InternalMessage&) { return true; })
    {
        return message_bus_->create_temp_message_await(co_executor_, msg_name, priority, std::move(filter));
    }

   protected:
    MessageBus<InternalMessage>* message_bus_;
    CoExecutor* co_executor_;
    TimerMgr* timer_mgr_;
    shared_ptr<Ros2Channel> ros2_ptr_;
    shared_ptr<MqttChannel> mqtt_ptr_;
};
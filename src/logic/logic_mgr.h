#pragma once

#include "logic.h"

class LogicMgr
{
   public:
    LogicMgr()
        : message_bus_(),
          co_executor_(10),
          timer_mgr_(&message_bus_, &co_executor_),
          ros2_ptr_(std::make_shared<Ros2Channel>("data_proxy", &message_bus_)),
          mqtt_ptr_(std::make_shared<MqttChannel>(&message_bus_))
    {
    }

    template <typename T>
    bool add_logic(const string& name)
    {
        if (all_logic_.contains(name))
        {
            return false;
        } else
        {
            all_logic_.insert({name, make_shared<T>(&co_executor_, &timer_mgr_, ros2_ptr_, mqtt_ptr_)});
            return true;
        }
    }

    void start()
    {
        ros2_ptr_->start();
        mqtt_ptr_->init("admin", "123456", "172.23.19.67");
        mqtt_ptr_->start();
        co_executor_.start();
        timer_mgr_.start();

        for (auto& [_, logic] : all_logic_)
        {
            logic->init();
        }

        message_bus_.start();
    }

   private:
    MessageBus<InternalMessage> message_bus_;
    CoExecutor co_executor_;
    TimerMgr timer_mgr_;
    shared_ptr<Ros2Channel> ros2_ptr_;
    shared_ptr<MqttChannel> mqtt_ptr_;
    std::unordered_map<string, shared_ptr<ILogic>> all_logic_;
};
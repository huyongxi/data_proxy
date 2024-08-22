#pragma once
#include "logic.h"
#include "co_task.h"



class SchedulerLogic : public ILogic
{
    public:
    virtual bool init() override;
    using ILogic::ILogic;

    CoTask handle_ros2_test_topic();
    CoTask handle_mqtt_test_topic();
};

#pragma once
#include "data_proxy/common.h"

class ThreadTask
{
    public:
    ThreadTask() = default;
    ThreadTask(const ThreadTask&) = delete;
    ThreadTask& operator=(const ThreadTask&) = delete;

    virtual ~ThreadTask()
    {
        thread_.join();
    }
    void start()
    {
        thread_ = std::thread([this](){this->run();});
    }
    private:
    virtual void run() = 0;

    private:
    std::thread thread_;
};


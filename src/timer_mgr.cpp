#include "timer_mgr.h"



void timer_callback(shared_ptr<steady_timer> timer, uint32_t ms, const function<bool()>& func, const error_code& ec)
{
    if (ec)
    {
        return;
    }
    if (func())
    {
        timer->expires_at(timer->expiry() + std::chrono::milliseconds(ms));
        timer->async_wait(std::bind(timer_callback, timer, ms, func, std::placeholders::_1));
    }
}

shared_ptr<steady_timer> start_timer(io_context& ioc, const function<bool()>& func, uint32_t ms)
{
    auto timer = make_shared<steady_timer>(ioc, std::chrono::milliseconds(ms));
    timer->async_wait(std::bind(timer_callback, timer, ms, func, std::placeholders::_1));
    return timer;
}


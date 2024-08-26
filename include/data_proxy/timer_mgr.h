#pragma once

#include <asio/io_context.hpp>
#include <asio/steady_timer.hpp>
#include <system_error>
#include "utils.h"
#include "messagebus.h"
#include "thread_task.h"

using asio::io_context;
using asio::steady_timer;
using std::error_code;

void timer_callback(shared_ptr<steady_timer> timer, uint32_t ms, const function<bool()>& func, const error_code& ec);
shared_ptr<steady_timer> start_timer(io_context& ioc, const function<bool()>& func, uint32_t ms);

class TimerMgr : public ThreadTask
{
   public:
    TimerMgr(MessageBus<InternalMessage>* message_bus, CoExecutor* co_executor)
        : message_bus_(message_bus), co_executor_(co_executor)
    {
        co_handle_remove_timer();
    }
    virtual void run() override { io_context_.run(); }

    uint64_t create_timer(uint32_t ms, const function<bool()>& func)
    {
        std::lock_guard lk(timers_mutex_);
        auto id = get_id();
        timers[id] = start_timer(io_context_, func, ms);
        return id;
    }

    std::pair<TempMessageAwait<InternalMessage>, uint64_t> create_time_await(uint32_t ms, bool isloop = true)
    {
        std::lock_guard lk(timers_mutex_);
        auto id = get_id();
        string msg_name = fmt::format("__T-{}", id);
        timers[id] = start_timer(
            io_context_,
            [=, this]()
            {
                InternalMessage imsg;
                imsg.data = msg_name;
                message_bus_->push_timer_message(std::move(imsg));
                return isloop;
            },
            ms);

        return {message_bus_->create_temp_message_await(co_executor_, msg_name, 10,
                                                        [](const InternalMessage&) { return true; }),
                id};
    }

    bool cancel_timer(uint64_t id)
    {
        std::lock_guard lk(timers_mutex_);
        auto it = timers.find(id);
        if (it != timers.end())
        {
            it->second->cancel();
            timers.erase(it);
            return true;
        }
        return false;
    }

    CoTask co_handle_remove_timer()
    {
        auto await = message_bus_->create_message_await(co_executor_, "__RemoveTimer", 10);
        while(true)
        {
            auto imsg = co_await await;
            uint64_t id = String2Int<uint64_t>(imsg.data);
            cancel_timer(id);
        }
    }

   private:
    uint64_t get_id() { return ++sid_; }

   private:
    asio::io_context io_context_;
    uint64_t sid_ = 0;
    std::unordered_map<uint64_t, shared_ptr<asio::steady_timer>> timers;
    mutex timers_mutex_;
    MessageBus<InternalMessage>* message_bus_;
    CoExecutor* co_executor_;
};

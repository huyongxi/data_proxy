#pragma once
#include <cstdint>
#include <exception>
#include <atomic>
#include <fmt/core.h>
#include <iostream>
#include "coroutine.h"

#if __has_builtin(__builtin_source_location)
#include <source_location>
using std::source_location;
#else
#include <experimental/source_location>
using std::experimental::source_location;
#endif


using std::coroutine_handle;
using std::suspend_always;
using std::suspend_never;


enum class CoState : uint8_t
{
    RuningState = 0,
    ExceptionState = 1,
    StopState = 2,
    QueueResume = 3,
};


class CoTask
{
    public:
    void resume()
    {
        coroutine_handle<CoTask::promise_type>::from_promise(promise_).resume();
    }

    class promise_type
    {
        public:
        promise_type(const source_location location = source_location::current())
        {
            std::cout << fmt::format("create coroutine {} from {}:{}:{}", (void*)this, location.file_name(),
                      location.line(), location.function_name()) << std::endl;
        }
        ~promise_type()
        {
            std::cout << fmt::format("destroy coroutine {}", (void*)this) << std::endl;
        }
        
        CoTask get_return_object()
        {
            return {*this};
        }

        suspend_never initial_suspend()
        {
            return {};
        }

        void return_void()
        {

        }
        suspend_always yield_value(int)
        {
            return {};
        }

        void unhandled_exception()
        {
            exception_ = std::current_exception();
        }

        suspend_never final_suspend() noexcept
        {
            return {};
        }

        // template <typename U>
        // U&& await_transform(U&& awaitable) noexcept
        // {
        //     return static_cast<U&&>(awaitable);
        // }

        std::exception_ptr exception_;
        std::atomic<CoState> state_{CoState::RuningState};
        void* await = nullptr;
    };
    promise_type& promise_;
};

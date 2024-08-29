#pragma once
#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <shared_mutex>
#include <string>
#include <thread>
#include <unordered_map>

#include "co_task.h"
#include "concurrentqueue.h"
#include "data_proxy/common.h"

using CoHandleWithPriority = std::pair<coroutine_handle<CoTask::promise_type>, uint8_t>;
template <>
struct std::less<CoHandleWithPriority>
{
    bool operator()(const CoHandleWithPriority& lhs, const CoHandleWithPriority& rhs) const
    {
        return lhs.second < rhs.second;
    }
};

class CoExecutor
{
   public:
    CoExecutor(int thread_num) : thread_num_(thread_num) {}
    void start()
    {
        if (thread_num_ < 1) thread_num_ = 1;
        thread_pool_.reserve(thread_num_);
        for (int i = 0; i < thread_num_; ++i)
        {
            thread_pool_.emplace_back([this]() { loop_resume_coroutine(); });
        }
    }

    void stop()
    {
        {
            std::lock_guard lk(mutex_);
            stop_ = true;
        }
        cv_.notify_all();
    }

    ~CoExecutor()
    {
        for (auto& t : thread_pool_)
        {
            t.join();
        }
    }

    void resume_coroutine(const coroutine_handle<CoTask::promise_type>& handle, uint8_t priority = 0)
    {
        handle.promise().state_ = CoState::QueueResume;
        {
            std::lock_guard lk(mutex_);
            queue_.push({handle, priority});
            if (wait_thread_num > 0)
            {
                cv_.notify_one();
            }
        }
    }

   private:
    void loop_resume_coroutine()
    {
        CoHandleWithPriority co_handle;
        while (true)
        {
            std::unique_lock lk(mutex_);
            ++wait_thread_num;
            cv_.wait(lk, [this]() { return queue_.size() > 0 || stop_; });
            --wait_thread_num;
            if (stop_)
            {
                lk.unlock();
                return;
            }
            co_handle = queue_.top();
            queue_.pop();
            lk.unlock();
            co_handle.first.resume();
        }
    }

   private:
    std::priority_queue<CoHandleWithPriority> queue_;
    std::vector<std::thread> thread_pool_;
    int thread_num_;
    std::mutex mutex_;
    std::condition_variable cv_;
    uint16_t wait_thread_num = 0;
    bool stop_ = false;
};

template <typename T>
class MessageBus;

template <typename T>
class SharedMessageAwait
{
   public:
    SharedMessageAwait(const SharedMessageAwait&) = delete;
    SharedMessageAwait& operator=(const SharedMessageAwait&) = delete;

    bool await_ready() { return queue_->try_dequeue(data_); }
    void await_suspend(coroutine_handle<CoTask::promise_type> handle)
    {
        handle_ = handle;
        handle_.promise().state_ = CoState::StopState;
        handle_.promise().await = this;
    }

    T&& await_resume()
    {
        handle_.promise().state_ = CoState::RuningState;
        return std::move(data_);
    }
    ~SharedMessageAwait();

   private:
    using IterType = typename std::unordered_multimap<std::string, SharedMessageAwait<T>*>::const_iterator;
    SharedMessageAwait(MessageBus<T>* message_bus, CoExecutor* co_executor, const std::string& wait_message_name,
                       uint8_t priority, std::shared_ptr<moodycamel::ConcurrentQueue<T>> queue = nullptr);
    SharedMessageAwait<T> clone()
    {
        return {message_bus_, co_executor_, wait_message_name_, wait_co_priority_, queue_};
    }

    bool push_message(T data) { return queue_->enqueue(std::move(data)); }
    bool resume_one_coroutine(const T& data)
    {
        if (!handle_ || handle_.promise().await != this)
        {
            return false;
        }
        if (handle_.promise().state_ == CoState::StopState)
        {
            data_ = data;
            co_executor_->resume_coroutine(handle_, wait_co_priority_);
            return true;
        }
        return false;
    }
    friend class MessageBus<T>;
    friend class TimerMgr;
    std::shared_ptr<moodycamel::ConcurrentQueue<T>> queue_;
    MessageBus<T>* message_bus_ = nullptr;
    CoExecutor* co_executor_ = nullptr;
    std::string wait_message_name_;
    coroutine_handle<CoTask::promise_type> handle_;
    bool suspend_ = false;
    T data_;
    IterType iter_;
    uint8_t wait_co_priority_ = 0;
};
template <typename T>
class MessageAwait
{
   public:
    MessageAwait(const MessageAwait&) = delete;
    MessageAwait& operator=(const MessageAwait&) = delete;
    bool await_ready() { return queue_.try_dequeue(data_); }
    void await_suspend(coroutine_handle<CoTask::promise_type> handle)
    {
        suspend_ = true;
        handle_ = handle;
        handle_.promise().state_ = CoState::StopState;
        handle_.promise().await = this;
    }

    T&& await_resume()
    {
        if (suspend_)
        {
            int try_dq = 0;
            while (!queue_.try_dequeue(data_))
            {
                ++try_dq;
            };
            if (try_dq > 0) std::cout << "try_dq: " << try_dq << std::endl;
        }
        suspend_ = false;
        handle_.promise().state_ = CoState::RuningState;
        return std::move(data_);
    }

    ~MessageAwait();

   private:
    using IterType = typename std::unordered_multimap<std::string, MessageAwait<T>*>::const_iterator;

    MessageAwait(MessageBus<T>* message_bus, CoExecutor* co_executor, const std::string& wait_message_name,
                 uint8_t priority);

    bool push_message(T data)
    {
        if (!handle_) return false;
        bool r = queue_.enqueue(std::move(data));
        if (handle_.promise().state_ == CoState::StopState && handle_.promise().await == this)
        {
            co_executor_->resume_coroutine(handle_, wait_co_priority_);
        }
        return r;
    }
    friend class MessageBus<T>;
    friend class TimerMgr;
    moodycamel::ConcurrentQueue<T> queue_;
    MessageBus<T>* message_bus_ = nullptr;
    CoExecutor* co_executor_ = nullptr;
    std::string wait_message_name_;
    coroutine_handle<CoTask::promise_type> handle_;
    bool suspend_ = false;
    T data_;
    IterType iter_;
    uint8_t wait_co_priority_ = 0;
};
template <typename T>
class TempMessageAwait
{
   public:
    TempMessageAwait(const TempMessageAwait&) = delete;
    TempMessageAwait& operator=(const TempMessageAwait&) = delete;

    bool await_ready() { return false; }
    void await_suspend(coroutine_handle<CoTask::promise_type> handle)
    {
        handle_ = handle;
        handle_.promise().state_ = CoState::StopState;
        handle_.promise().await = this;
    }

    T&& await_resume()
    {
        handle_.promise().state_ = CoState::RuningState;
        return std::move(data_);
    }
    ~TempMessageAwait();

   private:
    using IterType = typename std::unordered_multimap<std::string, TempMessageAwait<T>*>::const_iterator;
    TempMessageAwait(MessageBus<T>* message_bus, CoExecutor* co_executor, const std::string& wait_message_name,
                     uint8_t priority, std::function<bool(const T&)>&& filter);

    bool push_message(T data)
    {
        if (!filter_(data) && !is_timeout_msg(data)) return false;
        if (!handle_) return false;
        if (handle_.promise().state_ == CoState::StopState && handle_.promise().await == this)
        {
            data_ = std::move(data);
            co_executor_->resume_coroutine(handle_, wait_co_priority_);
            remove_timer();
            return true;
        }
        return false;
    }

    bool is_timeout_msg(const T& msg)
    {
        if constexpr (std::is_same_v<T, InternalMessage>)
        {
            return msg.is_timeout_msg;
        }
        return false;
    }

    void remove_timer()
    {
        if (timer_id_ > 0)
        {
            InternalMessage imsg;
            imsg.name = "__RemoveTimer";
            imsg.data = std::to_string(timer_id_);
            message_bus_->push_message(std::move(imsg));
        }
    }

    friend class MessageBus<T>;
    friend class TimerMgr;
    MessageBus<T>* message_bus_ = nullptr;
    CoExecutor* co_executor_ = nullptr;
    std::string wait_message_name_;
    coroutine_handle<CoTask::promise_type> handle_;
    T data_;
    IterType iter_;
    uint8_t wait_co_priority_ = 0;
    std::function<bool(const T& msg)> filter_;
    uint64_t timer_id_ = 0;
};

enum class AwaitType : uint8_t
{
    Invalid = 0,
    Shared = 1,
    Temp = 2,
    Normal = 3,
};

template <typename T>
struct AwaitTypeTraits
{
    const static AwaitType value = AwaitType::Invalid;
};

template <typename T>
struct AwaitTypeTraits<SharedMessageAwait<T>>
{
    const static AwaitType value = AwaitType::Shared;
};
template <typename T>
struct AwaitTypeTraits<MessageAwait<T>>
{
    const static AwaitType value = AwaitType::Normal;
};
template <typename T>
struct AwaitTypeTraits<TempMessageAwait<T>>
{
    const static AwaitType value = AwaitType::Temp;
};

template <typename T>
class MessageBus
{
   public:
    MessageBus() = default;
    MessageBus(const MessageBus&) = delete;
    MessageBus& operator=(const MessageBus&) = delete;

    ~MessageBus()
    {
        if (!stop_)
        {
            stop();
        }
        if (dispatch_thread_.joinable()) dispatch_thread_.join();
        if (high_priority_msg_dispatch_thread_.joinable()) high_priority_msg_dispatch_thread_.join();
    }

    SharedMessageAwait<T> create_shared_message_await(CoExecutor* co_executor, const std::string& wait_message_name,
                                                      uint8_t priority)
    {
        std::unique_lock lk(shared_message_await_map_mutex_);
        if (auto it = shared_message_await_map_.find(wait_message_name); it != shared_message_await_map_.end())
        {
            return it->second->clone();
        }
        return {this, co_executor, wait_message_name, priority};
    }
    MessageAwait<T> create_message_await(CoExecutor* co_executor, const std::string& wait_message_name,
                                         uint8_t priority)
    {
        std::unique_lock lk(message_await_map_mutex_);
        return {this, co_executor, wait_message_name, priority};
    }
    TempMessageAwait<T> create_temp_message_await(CoExecutor* co_executor, const std::string& wait_message_name,
                                                  uint8_t priority, std::function<bool(const T&)>&& filter)
    {
        std::unique_lock lk(temp_message_await_map_mutex_);
        return {this, co_executor, wait_message_name, priority, std::move(filter)};
    }

    template <typename Await>
    typename Await::IterType add_await(Await* await)
    {
        if constexpr (AwaitTypeTraits<Await>::value == AwaitType::Shared)
        {
            return shared_message_await_map_.insert({await->wait_message_name_, await});
        } else if constexpr (AwaitTypeTraits<Await>::value == AwaitType::Normal)
        {
            return message_await_map_.insert({await->wait_message_name_, await});
        } else if constexpr (AwaitTypeTraits<Await>::value == AwaitType::Temp)
        {
            return temp_message_await_map_.insert({await->wait_message_name_, await});
        }
        assert(false);
        return {};
    }
    template <typename Await>
    void remove_await(typename Await::IterType iter)
    {
        if constexpr (AwaitTypeTraits<Await>::value == AwaitType::Shared)
        {
            std::unique_lock lk(shared_message_await_map_mutex_);
            shared_message_await_map_.erase(iter);
        } else if constexpr (AwaitTypeTraits<Await>::value == AwaitType::Normal)
        {
            std::unique_lock lk(message_await_map_mutex_);
            message_await_map_.erase(iter);
        } else if constexpr (AwaitTypeTraits<Await>::value == AwaitType::Temp)
        {
            std::unique_lock lk(temp_message_await_map_mutex_);
            temp_message_await_map_.erase(iter);
        }
        return;
    }

    bool push_message(T&& data)
    {
        bool r = queue_.enqueue(std::move(data));
        if (r)
        {
            cv_.notify_all();
        }
        return r;
    }

    bool push_high_priority_message(T&& data)
    {
        bool r = high_priority_queue_.enqueue(std::move(data));
        if (r)
        {
            cv_.notify_all();
        }
        return r;
    }

    void run(moodycamel::ConcurrentQueue<T>& queue)
    {
        CoTask co_task = dispatch_message(queue);
        while (!stop_)
        {
            std::unique_lock lk(mutex_);
            cv_.wait(lk, [&, this]() { return queue.size_approx() > 0 || stop_; });
            lk.unlock();
            co_task.resume();
        }
    }

    void start()
    {
        dispatch_thread_ = std::thread([this]() { run(queue_); });
        high_priority_msg_dispatch_thread_ = std::thread([this]() { run(high_priority_queue_); });
        dispatch_thread_.join();
        high_priority_msg_dispatch_thread_.join();
    }
    void stop()
    {
        stop_ = true;
        cv_.notify_all();
    }

   private:
    CoTask dispatch_message(moodycamel::ConcurrentQueue<T>& queue)
    {
        T data;
        while (!stop_)
        {
            bool r = queue.try_dequeue(data);
            if (r)
            {
                {
                    std::shared_lock lk(shared_message_await_map_mutex_);
                    if (shared_message_await_map_.contains(data.name))
                    {
                        auto range = shared_message_await_map_.equal_range(data.name);
                        bool resume_one = false;
                        for (auto it = range.first; it != range.second; ++it)
                        {
                            if ((resume_one = it->second->resume_one_coroutine(data)))
                            {
                                break;
                            }
                        }
                        if (!resume_one)
                        {
                            range.first->second->push_message(data);
                        }
                    }
                }
                {
                    std::shared_lock lk(message_await_map_mutex_);
                    if (message_await_map_.contains(data.name))
                    {
                        auto range = message_await_map_.equal_range(data.name);
                        for (auto it = range.first; it != range.second; ++it)
                        {
                            it->second->push_message(data);
                        }
                    }
                }
                {
                    std::shared_lock lk(temp_message_await_map_mutex_);
                    if (temp_message_await_map_.contains(data.name))
                    {
                        auto range = temp_message_await_map_.equal_range(data.name);
                        for (auto it = range.first; it != range.second; ++it)
                        {
                            it->second->push_message(data);
                        }
                    }
                }
            } else
            {
                co_await std::suspend_always();
            }
        }
    }

   private:
    moodycamel::ConcurrentQueue<T> queue_;
    moodycamel::ConcurrentQueue<T> high_priority_queue_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::atomic<bool> stop_ = false;
    std::shared_mutex shared_message_await_map_mutex_;
    std::unordered_multimap<std::string, SharedMessageAwait<T>*> shared_message_await_map_;
    std::shared_mutex message_await_map_mutex_;
    std::unordered_multimap<std::string, MessageAwait<T>*> message_await_map_;
    std::shared_mutex temp_message_await_map_mutex_;
    std::unordered_multimap<std::string, TempMessageAwait<T>*> temp_message_await_map_;
    std::thread dispatch_thread_;
    std::thread high_priority_msg_dispatch_thread_;
};

#pragma once
#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>
#include <shared_mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <functional>
#include "co_task.h"
#include "concurrentqueue.h"
#include "coroutine.h"

using std::coroutine_handle;

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

    bool resume_coroutine(const coroutine_handle<CoTask::promise_type>& handle, uint8_t priority = 0)
    {
        if (handle.promise().state_ == CoState::StopState)
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
            return true;
        } else
        {
            return false;
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
    bool await_ready() { return queue_->try_dequeue(data_); }
    void await_suspend(coroutine_handle<CoTask::promise_type> handle)
    {
        handle_ = handle;
        handle_.promise().state_ = CoState::StopState;
        handle_.promise().await = this;
    }

    T&& await_resume()
    {
        handle_.promise().state_ = CoState::NormalState;
        return std::move(data_);
    }
    ~SharedMessageAwait();

   private:
    using IterType = typename std::unordered_multimap<std::string, SharedMessageAwait<T>*>::const_iterator;
    SharedMessageAwait(MessageBus<T>* message_bus, CoExecutor* co_executor, const std::string& wait_message_name,
                       std::shared_ptr<moodycamel::ConcurrentQueue<T>> queue = nullptr);
    SharedMessageAwait<T> clone() { return {message_bus_, co_executor_, wait_message_name_, queue_}; }

    bool push_message(T data) { return queue_->enqueue(std::move(data)); }
    bool resume_one_coroutine(const T& data)
    {
        if (!handle_ || handle_.promise().await != this)
        {
            return false;
        }
        bool r = co_executor_->resume_coroutine(handle_);
        if (r)
        {
            data_ = data;
        }
        return r;
    }
    friend class MessageBus<T>;
    std::shared_ptr<moodycamel::ConcurrentQueue<T>> queue_;
    MessageBus<T>* message_bus_ = nullptr;
    CoExecutor* co_executor_ = nullptr;
    std::string wait_message_name_;
    coroutine_handle<CoTask::promise_type> handle_;
    bool suspend_ = false;
    T data_;
    IterType iter_;
};
template <typename T>
class MessageAwait
{
   public:
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
            queue_.try_dequeue(data_);
        }
        suspend_ = false;
        handle_.promise().state_ = CoState::NormalState;
        return std::move(data_);
    }

    ~MessageAwait();

   private:
    using IterType = typename std::unordered_multimap<std::string, MessageAwait<T>*>::const_iterator;

    MessageAwait(MessageBus<T>* message_bus, CoExecutor* co_executor, const std::string& wait_message_name);

    bool push_message(T data)
    {
        if (!handle_) return false;
        bool r = queue_.enqueue(std::move(data));
        if (handle_.promise().await == this)
        {
            co_executor_->resume_coroutine(handle_);
        }
        return r;
    }
    friend class MessageBus<T>;
    moodycamel::ConcurrentQueue<T> queue_;
    MessageBus<T>* message_bus_ = nullptr;
    CoExecutor* co_executor_ = nullptr;
    std::string wait_message_name_;
    coroutine_handle<CoTask::promise_type> handle_;
    bool suspend_ = false;
    T data_;
    IterType iter_;
};
template <typename T>
class TempMessageAwait
{
   public:
    bool await_ready() { return false; }
    void await_suspend(coroutine_handle<CoTask::promise_type> handle)
    {
        handle_ = handle;
        handle_.promise().state_ = CoState::StopState;
        handle_.promise().await = this;
    }

    T&& await_resume()
    {
        handle_.promise().state_ = CoState::NormalState;
        return std::move(data_);
    }
    ~TempMessageAwait();

   private:
    using IterType = typename std::unordered_multimap<std::string, TempMessageAwait<T>*>::const_iterator;
    TempMessageAwait(MessageBus<T>* message_bus, CoExecutor* co_executor, const std::string& wait_message_name,
                     std::function<bool(const T&)>&& filter);

    bool push_message(T data)
    {
        if (!filter_(data)) return false;
        if (!handle_) return false;
        data_ = std::move(data);
        if (handle_.promise().await == this)
        {
            co_executor_->resume_coroutine(handle_);
        }
        return true;
    }
    friend class MessageBus<T>;
    MessageBus<T>* message_bus_ = nullptr;
    CoExecutor* co_executor_ = nullptr;
    std::string wait_message_name_;
    coroutine_handle<CoTask::promise_type> handle_;
    std::function<bool(const T& msg)> filter_;
    T data_;
    IterType iter_;
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
    ~MessageBus() = default;
    MessageBus(const MessageBus&) = delete;
    MessageBus& operator=(const MessageBus&) = delete;

    SharedMessageAwait<T> create_shared_message_await(CoExecutor* co_executor, const std::string& wait_message_name)
    {
        std::unique_lock lk(shared_message_await_map_mutex_);
        if (auto it = shared_message_await_map_.find(wait_message_name); it != shared_message_await_map_.end())
        {
            return it->second->clone();
        }
        return {this, co_executor, wait_message_name};
    }
    MessageAwait<T> create_message_await(CoExecutor* co_executor, const std::string& wait_message_name)
    {
        std::unique_lock lk(message_await_map_mutex_);
        return {this, co_executor, wait_message_name};
    }
    TempMessageAwait<T> create_temp_message_await(CoExecutor* co_executor, const std::string& wait_message_name, std::function<bool(const T&)>&& filter)
    {
        std::unique_lock lk(temp_message_await_map_mutex_);
        return {this, co_executor, wait_message_name, std::move(filter)};
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
        if (suspend_co_num_ > 0 && r)
        {
            cv_.notify_one();
        }
        return r;
    }

    void run()
    {
        CoTask co_task = dispatch_message();
        while (!stop_)
        {
            std::unique_lock lk(mutex_);
            cv_.wait(lk, [this]() { return queue_.size_approx() > 0 || stop_; });
            lk.unlock();
            co_task.resume();
        }
    }
    void stop()
    {
        stop_ = true;
        cv_.notify_all();
    }

   private:
    CoTask dispatch_message()
    {
        T data;
        while (!stop_)
        {
            bool r = queue_.try_dequeue(data);
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
                            if (resume_one = it->second->resume_one_coroutine(data) || resume_one; resume_one) break;
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
                ++suspend_co_num_;
                co_await StopAwait();
                --suspend_co_num_;
            }
        }
    }

   private:
    moodycamel::ConcurrentQueue<T> queue_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::atomic<uint16_t> suspend_co_num_ = 0;
    std::atomic<bool> stop_ = false;
    std::shared_mutex shared_message_await_map_mutex_;
    std::unordered_multimap<std::string, SharedMessageAwait<T>*> shared_message_await_map_;
    std::shared_mutex message_await_map_mutex_;
    std::unordered_multimap<std::string, MessageAwait<T>*> message_await_map_;
    std::shared_mutex temp_message_await_map_mutex_;
    std::unordered_multimap<std::string, TempMessageAwait<T>*> temp_message_await_map_;
};
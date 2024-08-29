#include "messagebus.h"

template <typename T>
SharedMessageAwait<T>::SharedMessageAwait(MessageBus<T>* message_bus, CoExecutor* co_executor,
                                          const std::string& wait_message_name, uint8_t priority,
                                          std::shared_ptr<moodycamel::ConcurrentQueue<T>> queue)
    : message_bus_(message_bus),
      co_executor_(co_executor),
      wait_message_name_(wait_message_name),
      wait_co_priority_(priority)
{
    if (queue)
    {
        queue_ = queue;
    } else
    {
        queue_ = std::make_shared<moodycamel::ConcurrentQueue<T>>();
    }
    iter_ = message_bus_->add_await(this);
}

template <typename T>
SharedMessageAwait<T>::~SharedMessageAwait()
{
    message_bus_->template remove_await<SharedMessageAwait<T>>(iter_);
}

template <typename T>
MessageAwait<T>::MessageAwait(MessageBus<T>* message_bus, CoExecutor* co_executor, const std::string& wait_message_name,
                              uint8_t priority)
    : message_bus_(message_bus),
      co_executor_(co_executor),
      wait_message_name_(wait_message_name),
      wait_co_priority_(priority)
{
    iter_ = message_bus_->add_await(this);
}

template <typename T>
MessageAwait<T>::~MessageAwait()
{
    message_bus_->template remove_await<MessageAwait<T>>(iter_);
}

template <typename T>
TempMessageAwait<T>::TempMessageAwait(MessageBus<T>* message_bus, CoExecutor* co_executor,
                                      const std::string& wait_message_name, uint8_t priority,
                                      std::function<bool(const T&)>&& filter)
    : message_bus_(message_bus),
      co_executor_(co_executor),
      wait_message_name_(wait_message_name),
      wait_co_priority_(priority),
      filter_(std::move(filter))

{
    iter_ = message_bus_->add_await(this);
}

template <typename T>
TempMessageAwait<T>::~TempMessageAwait()
{
    message_bus_->template remove_await<TempMessageAwait<T>>(iter_);


    if (wait_message_name_.find("__T-") == 0)
    {
        InternalMessage imsg;
        imsg.name = "__RemoveTimer";
        imsg.data = wait_message_name_.substr(4);
        message_bus_->push_message(std::move(imsg));
    }
}

template class SharedMessageAwait<InternalMessage>;
template class MessageAwait<InternalMessage>;
template class TempMessageAwait<InternalMessage>;

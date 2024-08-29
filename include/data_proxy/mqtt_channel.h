#pragma once
#include <google/protobuf/message.h>
#include <mosquittopp.h>
#include <rclcpp/rclcpp.hpp>
#include "messagebus.h"
#include "thread_task.h"


#define LogAndReturn(r) if (r != MOSQ_ERR_SUCCESS) \
        {                                          \
            std::cout << "mqtt error" << mosqpp::strerror(r) << std::endl;\
            return false;                          \
        } 

class MqttChannel : public mosqpp::mosquittopp, public ThreadTask
{
    public:

    MqttChannel(MessageBus<InternalMessage>* message_bus):message_bus_(message_bus)
    {
       
    }
    virtual ~MqttChannel()
    {
        mosqpp::lib_cleanup();
    }

    bool init(const string& username, const string& passwd, const string& broker_ip, uint16_t port = 1883)
    {
        mosqpp::lib_init();
        int r = username_pw_set(username.c_str(), passwd.c_str());
        LogAndReturn(r);
        r = connect(broker_ip.c_str(), port);
        LogAndReturn(r);
        return r;
    }

    bool publish(const string& topic, const google::protobuf::Message* msg)
    {
        auto data = msg->SerializeAsString();
        return publish(topic, data);
    }

    bool publish(const string& topic, const string_view payload)
    {
        int r = mosqpp::mosquittopp::publish(nullptr, topic.c_str(), payload.size(), payload.data(), 1);
        LogAndReturn(r);
        return true;
    }

    bool subscribe(const string& topic)
    {
        if (subscribe_topics_.contains(topic))
        {
            std::cout << "subscribe already sub " << topic << std::endl;
            return true;
        }
        int r = mosqpp::mosquittopp::subscribe(nullptr, topic.c_str(), 1);
        LogAndReturn(r);
        if (r == MOSQ_ERR_SUCCESS)
        {
            subscribe_topics_.insert(topic);
        }
        return true;
    }

    virtual void on_connect(int rc) override
    {
        std::cout << "on connect " << mosqpp::connack_string(rc) << std::endl;
        if (disconnected_)
        {
            for (const auto& topic : subscribe_topics_)
            {
                subscribe(topic);
            }
        }
        return;
    }
	virtual void on_connect_with_flags(int /*rc*/, int /*flags*/) override {return;}
	virtual void on_disconnect(int rc) override
    {
        std::cout << "on connect " << mosqpp::connack_string(rc) << std::endl;
        disconnected_ = true;
        return;
    }
	virtual void on_publish(int /*mid*/) override {return;}
	virtual void on_message(const struct mosquitto_message* message) override 
    {
        InternalMessage imsg;
        imsg.name.assign(message->topic);
        imsg.data.assign(static_cast<char*>(message->payload), message->payloadlen);
        message_bus_->push_message(std::move(imsg));
        return;
    }
	virtual void on_subscribe(int /*mid*/, int /*qos_count*/, const int * /*granted_qos*/) override {return;}
	virtual void on_unsubscribe(int /*mid*/) override {return;}
	virtual void on_log(int /*level*/, const char * /*str*/) override {return;}
	virtual void on_error() override {return;}

    MessageBus<InternalMessage>* get_message_bus()
    {
       return message_bus_;
    }

    private:
    virtual void run() override
    {
        loop_forever();
    }

    private:
    MessageBus<InternalMessage>* message_bus_;
    std::unordered_set<string> subscribe_topics_;
    bool disconnected_ = false;
};




#ifndef DBC_TOPIC_MANAGER_H
#define DBC_TOPIC_MANAGER_H

#include "util/utils.h"
#include "../module/module.h"
#include "network/protocol/service_message.h"
#include "log/log.h"

extern const std::string topic_manager_name;

class topic_manager : public module
{
public:
    ~topic_manager() override { write_lock_guard<rw_lock> lock_guard(m_lock); m_topic_registry.clear(); }

    std::string module_name() const override { return topic_manager_name; }

    template<typename function_type>
    void subscribe(const std::string &topic, function_type &&f)
    {
        auto func = to_function(std::forward<function_type>(f));
        add(topic, std::move(func));
    }

    //publish topic to bus with args function
    template<typename ret_type, typename... args_type>
    void publish(const std::string &topic, args_type&&... args)
    {
        using function_type = std::function<ret_type (args_type...)>;
        std::string msg_type = topic + typeid(function_type).name();

        if (topic != "time_tick_notification")
        {
            LOG_DEBUG << "topic manager publish: "<<topic;
        }

        read_lock_guard<rw_lock> lock_guard(m_lock);
        auto range = m_topic_registry.equal_range(msg_type);
        if (range.first == range.second)
        {
            LOG_ERROR << "1 topic manager could not find topic invoke function: " << topic;
            return;
        }

        for (auto it = range.first; it != range.second; it++)
        {
            auto f = boost::any_cast<function_type>(it->second);
            f(std::forward<args_type>(args)...);
        }
    }

    //publish topic to bus with no args function
    template<typename function_type>
    void publish(const std::string &topic)
    {
//                using std_function_type = std::function<function_type()>;
        std::string msg_type = topic + typeid(function_type).name();

        read_lock_guard<rw_lock> lock_guard(m_lock);
        auto range = m_topic_registry.equal_range(msg_type);
        if (range.first == m_topic_registry.end())
        {
            LOG_ERROR << "2 topic manager could not find topic invoke function: " << topic;
            return;
        }

        for (auto it = range.first; it != range.second; it++)
        {
            auto f = boost::any_cast<function_type>(it->second);
            f();
        }
    }

    //unsubscribe
    template<typename ret_type, typename...args_type>
    void unsubscribe(const std::string &topic)
    {
        using function_type = std::function<ret_type(args_type...)>;
        std::string msg_type = topic + typeid(function_type).name();

        write_lock_guard<rw_lock> lock_guard(m_lock);
        int count = m_topic_registry.count(msg_type);
        if (0 == count)
        {
            return;
        }
        auto range = m_topic_registry.equal_range(msg_type);
        m_topic_registry.erase(range.first, range.second);
    }

protected:

    template<typename function_type>
    void add(const std::string &topic, function_type &&func)
    {
        std::string msg_type = topic + typeid(function_type).name();

        write_lock_guard<rw_lock> lock_guard(m_lock);
        m_topic_registry.emplace(std::move(msg_type), std::forward<function_type>(func));
    }

protected:
    rw_lock m_lock;
    std::multimap<std::string, boost::any> m_topic_registry;
};

#endif 

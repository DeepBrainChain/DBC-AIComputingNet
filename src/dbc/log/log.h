#ifndef DBC_LOG_H
#define DBC_LOG_H

#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/sinks/text_file_backend.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/sources/severity_channel_logger.hpp>
#include <boost/log/utility/setup/console.hpp>  
#include <boost/log/utility/exception_handler.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/log/attributes/mutable_constant.hpp>
#include <boost/exception/diagnostic_information.hpp>
#include "util/utils.h"

namespace logging = boost::log;
namespace sinks = boost::log::sinks;
namespace src = boost::log::sources;
namespace expr = boost::log::expressions;
namespace attrs = boost::log::attributes;
namespace keywords = boost::log::keywords;

extern const char * get_short_file_name(const char * file_path);
extern const char* get_short_func_name(const char* func_name);

#ifndef LOG_DEBUG_INFO_ADDED
#define LOG_DEBUG_INFO_ADDED
#define __SHORT_FILE__        get_short_file_name(__FILE__)
#define __SHORT_FUNC__        get_short_func_name(__FUNCTION__)
#endif

#ifdef LOG_DEBUG_INFO_ADDED
#define LOG_TRACE               BOOST_LOG_TRIVIAL(trace) << __SHORT_FILE__ << " | " << __LINE__ << " | " << __SHORT_FUNC__  << " | "
#define LOG_DEBUG               BOOST_LOG_TRIVIAL(debug) << __SHORT_FILE__ << " | " << __LINE__ << " | " << __SHORT_FUNC__  << " | "
#define LOG_INFO                BOOST_LOG_TRIVIAL(info) << __SHORT_FILE__ << " | " << __LINE__ << " | " << __SHORT_FUNC__  << " | "
#define LOG_WARNING             BOOST_LOG_TRIVIAL(warning) << __SHORT_FILE__ << " | " << __LINE__ << " | " << __SHORT_FUNC__  << " | "
#define LOG_ERROR               BOOST_LOG_TRIVIAL(error) << __SHORT_FILE__ << " | " << __LINE__ << " | " << __SHORT_FUNC__  << " | "
#define LOG_FATAL               BOOST_LOG_TRIVIAL(fatal) << __SHORT_FILE__ << " | " << __LINE__ << " | " << __SHORT_FUNC__  << " | "
#else
#define LOG_TRACE               BOOST_LOG_TRIVIAL(trace)
#define LOG_DEBUG               BOOST_LOG_TRIVIAL(debug)
#define LOG_INFO                BOOST_LOG_TRIVIAL(info)
#define LOG_WARNING             BOOST_LOG_TRIVIAL(warning)
#define LOG_ERROR               BOOST_LOG_TRIVIAL(error)
#define LOG_FATAL               BOOST_LOG_TRIVIAL(fatal)
#endif



#define TASK_LOGGER(task, lvl, msg) \
    do { \
        auto dbclogger = dbclog::instance().get_task_logger(task); \
        if (dbclogger != nullptr) { \
            BOOST_LOG_SEV(*dbclogger.get(), ::boost::log::trivial::lvl) << msg; \
        } \
    } while(0)

#define TASK_LOG_TRACE(task, msg)    TASK_LOGGER(task, trace, msg)
#define TASK_LOG_DEBUG(task, msg)    TASK_LOGGER(task, debug, msg)
#define TASK_LOG_INFO(task, msg)     TASK_LOGGER(task, info, msg)
#define TASK_LOG_WARNING(task, msg)  TASK_LOGGER(task, warning, msg)
#define TASK_LOG_ERROR(task, msg)    TASK_LOGGER(task, error, msg)
#define TASK_LOG_FATAL(task, msg)    TASK_LOGGER(task, fatal, msg)

// Set attribute and return the new value
template<typename ValueType>
static ValueType set_get_attrib(const char* name, ValueType value) {
    auto attr = logging::attribute_cast<attrs::mutable_constant<ValueType>>(logging::core::get()->get_global_attributes()[name]);
    attr.set(value);
    return attr.get();
}

typedef src::severity_channel_logger_mt<logging::trivial::severity_level, std::string> logger_type;

class dbclog : public Singleton<dbclog>
{
public:
    dbclog() = default;
    ~dbclog() {};

    int32_t init();
    void set_filter_level(boost::log::trivial::severity_level level);

    // add boost log backend
    void add_task_log_backend(const std::string& task_id);

    std::shared_ptr<logger_type> get_task_logger(const std::string& task_id);

protected:
    std::map<std::string, std::shared_ptr<logger_type>> loggers;
};

#endif

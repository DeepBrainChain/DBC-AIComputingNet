#ifndef DBC_LOG_H
#define DBC_LOG_H

#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/sinks/text_file_backend.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/sources/severity_logger.hpp>
// #include <boost/log/sources/record_ostream.hpp>
#include <boost/log/utility/setup/console.hpp>  
#include <boost/log/utility/exception_handler.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/log/attributes/mutable_constant.hpp>
#include <boost/exception/diagnostic_information.hpp>
#include "common.h"
#include <string>
#include "common/util.h"


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
#define LOG_TRACE               BOOST_LOG_TRIVIAL(trace)        << __SHORT_FILE__ << " | " << __LINE__ << " | " << __SHORT_FUNC__  << " | "
#define LOG_DEBUG               BOOST_LOG_TRIVIAL(debug)     << __SHORT_FILE__ << " | " << __LINE__ << " | " << __SHORT_FUNC__  << " | "
#define LOG_INFO                   BOOST_LOG_TRIVIAL(info)          << __SHORT_FILE__ << " | " << __LINE__ << " | " << __SHORT_FUNC__  << " | "
#define LOG_WARNING         BOOST_LOG_TRIVIAL(warning) << __SHORT_FILE__ << " | " << __LINE__ << " | " << __SHORT_FUNC__  << " | "
#define LOG_ERROR               BOOST_LOG_TRIVIAL(error)       << __SHORT_FILE__ << " | " << __LINE__ << " | " << __SHORT_FUNC__  << " | "
#define LOG_FATAL                BOOST_LOG_TRIVIAL(fatal)         << __SHORT_FILE__ << " | " << __LINE__ << " | " << __SHORT_FUNC__  << " | "
#else
#define LOG_TRACE               BOOST_LOG_TRIVIAL(trace)
#define LOG_DEBUG              BOOST_LOG_TRIVIAL(debug)
#define LOG_INFO                  BOOST_LOG_TRIVIAL(info)
#define LOG_WARNING         BOOST_LOG_TRIVIAL(warning)
#define LOG_ERROR              BOOST_LOG_TRIVIAL(error)
#define LOG_FATAL               BOOST_LOG_TRIVIAL(fatal)
#endif

#if 0
#define LOG_DEBUG \
  BOOST_LOG_STREAM_WITH_PARAMS( \
     (::boost::log::trivial::logger::get()), \
        (matrix::core::set_get_attrib("File", matrix::core::path_to_filename(__FILE__))) \
        (matrix::core::set_get_attrib("Line", __LINE__)) \
        (::boost::log::keywords::severity = (boost::log::trivial::debug)) \
  )
#endif



namespace matrix
{
    namespace core
    {
        // Set attribute and return the new value
        template<typename ValueType>
        static ValueType set_get_attrib(const char* name, ValueType value) {
            auto attr = logging::attribute_cast<attrs::mutable_constant<ValueType>>(logging::core::get()->get_global_attributes()[name]);
            attr.set(value);
            return attr.get();
        }

#if 0
        // Convert file path to only the filename
        static std::string path_to_filename(std::string path) {
            return path.substr(path.find_last_of("/\\") + 1);
        }
#endif

        struct log_exception_handler
        {
            void operator() (std::runtime_error const& e) const
            {
                std::cout << "std::runtime_error: " << e.what() << std::endl;
            }
            void operator() (std::logic_error const& e) const
            {
                std::cout << "std::logic_error: " << e.what() << std::endl;
                throw;
            }
        };

        class log
        {
        public:

            static int32_t init()
            {
                boost::log::register_simple_formatter_factory< boost::log::trivial::severity_level, char >("Severity");
//#ifdef __DEBUG
//                logging::add_console_log(std::clog, keywords::format = "[%TimeStamp%][%Severity%]: %Message%");

//#endif
                try
                {
                    auto sink = logging::add_file_log
                    (
                        //attribute
                        keywords::file_name = (matrix::core::path_util::get_exe_dir() /= "logs/matrix_core_%Y%m%d%H%M%S_%N.log").c_str(),
                        keywords::target = (matrix::core::path_util::get_exe_dir() /= "logs"),
                        keywords::max_files = 10,
                        keywords::rotation_size = 100 * 1024 * 1024,
                        keywords::time_based_rotation = sinks::file::rotation_at_time_point(0, 0, 0),
                        keywords::format = (
                            expr::stream
                                    << expr::format_date_time<boost::posix_time::ptime>("TimeStamp", "%m-%d %H:%M:%S.%f")
                                    << "|" << expr::attr< attrs::current_thread_id::value_type>("ThreadID")
                                    << "|" << std::setw(7) << std::setfill(' ') << std::left << logging::trivial::severity
                                    << "|" << expr::smessage
                    )
                );

                    logging::core::get()->set_filter(boost::log::trivial::severity >= boost::log::trivial::trace);
                    sink->locked_backend()->auto_flush(true);

                    logging::core::get()->set_exception_handler(
                        logging::make_exception_handler<std::runtime_error, std::logic_error>(log_exception_handler())
                    );

                    logging::add_common_attributes();
                }

                catch (const std::exception & e)
                {
                    std::cout << "log error" << e.what() << std::endl;
                    return E_DEFAULT;
                }
                catch (const boost::exception & e)
                {
                    std::cout << "log error" << diagnostic_information(e) << std::endl;
                    return E_DEFAULT;
                }
                catch (...)
                {
                    std::cout << "log error" << std::endl;
                    return E_DEFAULT;
                }

                //fix by regulus:fix boost::log throw exception cause coredump where open files is not enough. can use the 2 method as below.
                // suppress log
                //logging::core::get()->set_exception_handler(logging::make_exception_suppressor());
                //use log_handler
                logging::core::get()->set_exception_handler(
                    logging::make_exception_handler<std::runtime_error, std::logic_error>(log_exception_handler())
                );
                //BOOST_LOG_TRIVIAL(info) << "init core log success.";

                return E_SUCCESS;
            }

            static void set_filter_level(boost::log::trivial::severity_level level)
            {
                switch (level)
                {
                case boost::log::trivial::trace:
                case boost::log::trivial::debug:
                case boost::log::trivial::info:
                case boost::log::trivial::warning:
                case boost::log::trivial::error:
                case boost::log::trivial::fatal:
                {
                    logging::core::get()->set_filter(boost::log::trivial::severity >= level);
                    break;
                }
                default:
                    break;
                }
            }

        };

    }

}


#endif

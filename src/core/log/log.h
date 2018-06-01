/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        log.h
* description    log for matrix core
* date                  :   2018.01.20
* author            Bruce Feng
**********************************************************************************/
#pragma once

#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/sinks/text_file_backend.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/sources/record_ostream.hpp>
#include <boost/log/utility/setup/console.hpp>  
#include <boost/log/utility/exception_handler.hpp>
#include <boost/log/support/date_time.hpp>
#include "common.h"


namespace logging = boost::log;
namespace sinks = boost::log::sinks;
namespace src = boost::log::sources;
namespace expr = boost::log::expressions;
namespace attrs = boost::log::attributes;
namespace keywords = boost::log::keywords;


#define LOG_TRACE               BOOST_LOG_TRIVIAL(trace)
#define LOG_DEBUG              BOOST_LOG_TRIVIAL(debug)
#define LOG_INFO                  BOOST_LOG_TRIVIAL(info)
#define LOG_WARNING         BOOST_LOG_TRIVIAL(warning)
#define LOG_ERROR              BOOST_LOG_TRIVIAL(error)
#define LOG_FATAL               BOOST_LOG_TRIVIAL(fatal)


namespace matrix
{
    namespace core
    {
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
                
                auto sink = logging::add_file_log
                (
                    //attribute
                    keywords::file_name = "matrix_core_%N.log",
                    keywords::rotation_size = 100 * 1024 * 1024,
                    keywords::time_based_rotation = sinks::file::rotation_at_time_point(0, 0, 0),
//                    keywords::format = "[%TimeStamp%][%Severity%]: %Message%"
                    keywords::format = (
                            expr::stream
                                    << expr::format_date_time<boost::posix_time::ptime>("TimeStamp", "%Y-%m-%d %H:%M:%S.%f")
                                    << " | " << expr::attr< attrs::current_thread_id::value_type>("ThreadID")
                                    << " | " << std::setw(7) << std::setfill(' ') << std::left << logging::trivial::severity
                                    << " | " << expr::smessage
                    )
                );

                logging::core::get()->set_filter(boost::log::trivial::severity >= boost::log::trivial::debug);
                sink->locked_backend()->auto_flush(true);
             
                logging::add_common_attributes();
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
            
        };

    }

}



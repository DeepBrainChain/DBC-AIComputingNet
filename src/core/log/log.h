/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        £ºlog.h
* description    £ºlog for matrix core
* date                  : 2018.01.20
* author            £ºBruce Feng
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
        
        class log
        {
        public:

            static int32_t init()
            {
                boost::log::register_simple_formatter_factory< boost::log::trivial::severity_level, char >("Severity");
#ifdef __DEBUG
                logging::add_console_log(std::clog, keywords::format = "[%TimeStamp%][%Severity%]: %Message%");
#endif
                
                auto sink = logging::add_file_log
                (
                    //attribute
                    keywords::file_name = "matrix_core_%N.log",
                    keywords::rotation_size = 100 * 1024 * 1024,
                    keywords::time_based_rotation = sinks::file::rotation_at_time_point(0, 0, 0),
                    keywords::format = "[%TimeStamp%][%Severity%]: %Message%"
                );

                logging::core::get()->set_filter(boost::log::trivial::severity >= boost::log::trivial::debug);
                sink->locked_backend()->auto_flush(true);
             
                logging::add_common_attributes();

                //BOOST_LOG_TRIVIAL(info) << "init core log success.";

                return E_SUCCESS;
            }
            
        };

    }

}



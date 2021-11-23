#ifndef _UNITTEST_LOG_ADD_FILE_LOG_H_
#define _UNITTEST_LOG_ADD_FILE_LOG_H_

#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/sinks/text_file_backend.hpp>
#include <boost/log/sinks/async_frontend.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/sources/record_ostream.hpp>
#include <boost/log/support/date_time.hpp>

namespace logging = boost::log;
namespace src = boost::log::sources;
namespace sinks = boost::log::sinks;
namespace keywords = boost::log::keywords;
namespace expr = boost::log::expressions;
namespace attrs = boost::log::attributes;

void log100times() {
  for (int i = 0; i < 100; i++) {
    BOOST_LOG_TRIVIAL(trace) << "A trace severity message";
    BOOST_LOG_TRIVIAL(debug) << "A debug severity message";
    BOOST_LOG_TRIVIAL(info) << "An informational severity message";
    BOOST_LOG_TRIVIAL(warning) << "A warning severity message";
    BOOST_LOG_TRIVIAL(error) << "An error severity message";
    BOOST_LOG_TRIVIAL(fatal) << "A fatal severity message";
  }
}

void add_file_log1() {
  auto sink = logging::add_file_log(
    keywords::file_name = "unittest_%Y%m%d%H%M%S_%N.log",                         /*< file name pattern >*/
    keywords::max_files = 10,
    keywords::rotation_size = 10 * 1024 * 1024,                                   /*< rotate files every 10 MiB... >*/
    keywords::time_based_rotation = sinks::file::rotation_at_time_point(0, 0, 0), /*< ...or at midnight >*/
    keywords::format = (
      expr::stream
        << expr::format_date_time<boost::posix_time::ptime>("TimeStamp", "%Y-%m-%d %H:%M:%S.%f")
        << " | " << expr::attr< attrs::current_thread_id::value_type>("ThreadID")
        << " | " << std::setw(7) << std::setfill(' ') << std::left << logging::trivial::severity
        << " | " << expr::smessage
    )
  );
  sink->locked_backend()->auto_flush(true);
  logging::add_common_attributes();
}

void add_sync_file_log() {
  boost::shared_ptr<logging::core> core = logging::core::get();

  boost::shared_ptr<sinks::text_file_backend> backend =
      boost::make_shared<sinks::text_file_backend>(
      keywords::file_name = "unittest_sync_%Y%m%d%H%M%S_%N.log",
      keywords::max_files = 10,
      keywords::rotation_size = 100 * 1024 * 1024,
      keywords::time_based_rotation = sinks::file::rotation_at_time_point(0, 0, 0)
      );

  typedef sinks::synchronous_sink<sinks::text_file_backend> sink_t;
  boost::shared_ptr<sink_t> sink(new sink_t(backend));
  sink->set_formatter(
      expr::stream
        << expr::format_date_time<boost::posix_time::ptime>("TimeStamp", "%Y-%m-%d %H:%M:%S.%f")
        << " | " << expr::attr< attrs::current_thread_id::value_type>("ThreadID")
        << " | " << std::setw(7) << std::setfill(' ') << std::left << logging::trivial::severity
        << " | " << expr::smessage);

  sink->locked_backend()->auto_flush(true);
  core->add_sink(sink);
  logging::add_common_attributes();
}

void add_async_file_log() {
  boost::shared_ptr<logging::core> core = logging::core::get();

  boost::shared_ptr<sinks::text_file_backend> backend =
      boost::make_shared<sinks::text_file_backend>(
      keywords::file_name = "unittest_async_%Y%m%d%H%M%S_%N.log",
      keywords::max_files = 10,
      keywords::rotation_size = 100 * 1024 * 1024,
      keywords::time_based_rotation = sinks::file::rotation_at_time_point(0, 0, 0)
      );

  typedef sinks::asynchronous_sink<sinks::text_file_backend> sink_t;
  boost::shared_ptr<sink_t> sink(new sink_t(backend));
  sink->set_formatter(
      expr::stream
        << expr::format_date_time<boost::posix_time::ptime>("TimeStamp", "%Y-%m-%d %H:%M:%S.%f")
        << " | " << expr::attr< attrs::current_thread_id::value_type>("ThreadID")
        << " | " << std::setw(7) << std::setfill(' ') << std::left << logging::trivial::severity
        << " | " << expr::smessage);

  sink->locked_backend()->auto_flush(true);
  core->add_sink(sink);
  logging::add_common_attributes();
}

#endif //_UNITTEST_LOG_ADD_FILE_LOG_H_

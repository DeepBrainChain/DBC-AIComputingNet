// The following two lines indicates boost test with Shared Library mode
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>


using namespace boost::unit_test;
using namespace std;


#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/sinks/text_file_backend.hpp>
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

void init()
{
    boost::log::register_simple_formatter_factory< boost::log::trivial::severity_level, char >("Severity");

    logging::add_file_log(
            keywords::file_name = "sample_%N.log",                                        /*< file name pattern >*/
            keywords::rotation_size = 10 * 1024 * 1024,                                   /*< rotate files every 10 MiB... >*/
            keywords::time_based_rotation = sinks::file::rotation_at_time_point(0, 0, 0), /*< ...or at midnight >*/
//            keywords::format = "%TimeStamp% | %ThreadID% | %Severity% | %Message%"
            keywords::format = (
                    expr::stream
                            << expr::format_date_time<boost::posix_time::ptime>("TimeStamp", "%Y-%m-%d %H:%M:%S.%f")
                            << " | " << expr::attr< attrs::current_thread_id::value_type>("ThreadID")
                            << " | " << std::setw(7) << std::setfill(' ') << std::left << logging::trivial::severity
                            << " | " << expr::smessage
            )
    );

    logging::core::get()->set_filter(
            logging::trivial::severity >= logging::trivial::info
    );
}

// [2018-05-31 21:31:35.712207] [0x00007fffabeb2380] [trace]   A trace severity message
BOOST_AUTO_TEST_CASE(trivial_log_test) {


    BOOST_LOG_TRIVIAL(trace) << "A trace severity message";
    BOOST_LOG_TRIVIAL(debug) << "A debug severity message";
    BOOST_LOG_TRIVIAL(info) << "An informational severity message";
    BOOST_LOG_TRIVIAL(warning) << "A warning severity message";
    BOOST_LOG_TRIVIAL(error) << "An error severity message";
    BOOST_LOG_TRIVIAL(fatal) << "A fatal severity message";

}


BOOST_AUTO_TEST_CASE(file_log_test) {

    init();
    logging::add_common_attributes();

//    BOOST_LOG_ATTRIBUTE_KEYWORD(severity, "Severity", severity_level);

    BOOST_LOG_TRIVIAL(trace) << "A trace severity message";
    BOOST_LOG_TRIVIAL(debug) << "A debug severity message";
    BOOST_LOG_TRIVIAL(info) << "An informational severity message";
    BOOST_LOG_TRIVIAL(warning) << "A warning severity message";
    BOOST_LOG_TRIVIAL(error) << "An error severity message";
    BOOST_LOG_TRIVIAL(fatal) << "A fatal severity message";

}




#include "log.h"

const char * get_short_file_name(const char * file_path)
{
    if (nullptr == file_path || '\0' == *file_path)
    {
        return "";
    }

    const char * p = file_path;
    const char *short_file_name = file_path;

    while (*p)
    {
        if ('\\' == *p || '/' == *p)
        {
            short_file_name = p + 1;
        }

        p++;
    }

    return short_file_name;
}

const char* get_short_func_name(const char* func_name)
{
    std::string str_fn = func_name;
    size_t pos = str_fn.rfind(":");
    if (pos != std::string::npos)
    {
        return func_name + pos + 1;
    }
    
    return func_name;
}

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
	void operator() (boost::exception const& e) const {
		std::cout << "boost.log::exception: " << diagnostic_information_what(e) << std::endl;
	}
};

int32_t dbclog::init()
{
    boost::log::register_simple_formatter_factory< boost::log::trivial::severity_level, char >("Severity");

    try
    {
        auto sink = logging::add_file_log
        (
            //attribute
            keywords::file_name = (util::get_exe_dir() /= "logs/matrix_core_%Y%m%d%H%M%S_%N.log").c_str(),
            keywords::target = (util::get_exe_dir() /= "logs"),
            keywords::max_files = 10,
            keywords::rotation_size = 100 * 1024 * 1024,
            keywords::time_based_rotation = sinks::file::rotation_at_time_point(0, 0, 0),
            keywords::format = (
                expr::stream
                    << expr::format_date_time<boost::posix_time::ptime>("TimeStamp", "%m-%d %H:%M:%S.%f")
                    << " | " << expr::attr< attrs::current_thread_id::value_type>("ThreadID")
                    << " | " << std::setw(7) << std::setfill(' ') << std::left << logging::trivial::severity
                    << " | " << expr::smessage
            )
        );

        logging::core::get()->set_filter(boost::log::trivial::severity >= boost::log::trivial::trace);
        sink->set_filter(!expr::has_attr<std::string>("Channel"));
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

    return ERR_SUCCESS;
}

void dbclog::set_filter_level(boost::log::trivial::severity_level level)
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

void dbclog::add_task_log_backend(const std::string& task_id)
{
    try {
        std::string exe_path = util::get_exe_dir().string();
        boost::shared_ptr<logging::core> core = logging::core::get();

        boost::shared_ptr<sinks::text_file_backend> backend =
            boost::make_shared<sinks::text_file_backend>(
            keywords::file_name = (exe_path + "/logs/task_" + task_id + "_%N.log").c_str(),
            keywords::target = (exe_path + "/logs").c_str(),
            keywords::max_files = 10,
            keywords::open_mode = std::ios_base::app | std::ios_base::out,
            keywords::rotation_size = 100 * 1024 * 1024
            );

        typedef sinks::synchronous_sink<sinks::text_file_backend> sink_t;
        boost::shared_ptr<sink_t> sink(new sink_t(backend));
        sink->set_formatter(
            expr::stream
                << expr::format_date_time<boost::posix_time::ptime>(
                    "TimeStamp", "%Y-%m-%d %H:%M:%S.%f")
                << " | "
                << expr::attr<std::string>("Channel")
                << " | "
                << std::setw(7) << std::setfill(' ') << std::left << logging::trivial::severity
                << " | "
                << expr::smessage);

        sink->set_filter(expr::attr<std::string>("Channel") == task_id);
        sink->locked_backend()->auto_flush(true);
        core->add_sink(sink);

        loggers[task_id] = std::make_shared<logger_type>(keywords::channel = task_id);
    }
    catch (const std::exception & e)
    {
        std::cout << "log error" << e.what() << std::endl;
        LOG_ERROR << "add task(" << task_id << ") log backend error: " << e.what();
    }
    catch (const boost::exception & e)
    {
        std::cout << "log error" << diagnostic_information(e) << std::endl;
        LOG_ERROR << "add task(" << task_id << ") log backend error: " << diagnostic_information(e);
    }
    catch (...)
    {
        std::cout << "log error" << std::endl;
        LOG_ERROR << "add task(" << task_id << ") log backend error";
    }
}

std::shared_ptr<logger_type> dbclog::get_task_logger(const std::string& task_id)
{
    auto iter = loggers.find(task_id);
    if (iter != loggers.end()) {
        return iter->second;
    }
    LOG_ERROR << "can not find task log backend " << task_id;
    return nullptr;
}

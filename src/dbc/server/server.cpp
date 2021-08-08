#include "server.h"

#include <fcntl.h>
#include "util/crypto/byteswap.h"
#include "network/connection_manager.h"
#include "config/env_manager.h"
#include "service_module/topic_manager.h"
#include "util/utils/crypto_service.h"
#include <boost/exception/all.hpp>
#include "service_module/service_name.h"
#include "data/resource/SystemResourceManager.h"

#include "service/cmd_request_service/cmd_request_service.h"
#include "service/http_request_service/http_server_service.h"
#include "service/http_request_service/rest_api_service.h"
#include "service/node_request_service/data_query_service.h"
#include "service/node_request_service/node_request_service.h"
#include "service/peer_request_service/p2p_net_service.h"

#define DEFAULT_SLEEP_MILLI_SECONDS                 1000

int32_t server::init(int argc, char *argv[]) {
    LOG_INFO << "begin to init dbc core";

    struct timeval t_start{};
    gettimeofday(&t_start, nullptr);

    int32_t ret = E_SUCCESS;
    variables_map vm;

    // crypto service
    LOG_INFO << "begin to init crypto service";
    ret = m_crypto.init(vm);
    if (E_SUCCESS != ret) {
        LOG_ERROR << "crypto service init failed";
        return ret;
    }
    LOG_INFO << "init crypto service successfully";

    // parse command line
    LOG_INFO << "begin to parse command line";
    ret = parse_command_line(argc, argv, vm);
    if (E_SUCCESS != ret) {
        LOG_ERROR << "parse command line failed";
        return ret;
    }
    LOG_INFO << "parse command line successfully";

    // env_manager
    LOG_INFO << "begin to init env manager";
    ret = env_manager::instance().init(vm);
    if (E_SUCCESS != ret) {
        LOG_ERROR << "init env manager failed";
        return ret;
    }
    LOG_INFO << "init env manager successfully";

    // conf_manager
    LOG_INFO << "begin to init conf manager";
    ret = conf_manager::instance().init(vm);
    if (E_SUCCESS != ret) {
        LOG_ERROR << "init conf manager failed";
        return ret;
    }
    LOG_INFO << "init conf manager successfully";

    // system_resource_manager
    if (vm.count(SERVICE_NAME_AI_TRAINING)) {
        LOG_INFO << "begin to init system resource manager";
        SystemResourceMgr::instance().Init();
        LOG_INFO << "init system resource manager successfully";
    }

    // timer_matrix_manager
    LOG_INFO << "begin to init timer matrix manager";
    m_timer_matrix_manager = std::make_shared<timer_matrix_manager>();
    ret = m_timer_matrix_manager->init(vm);
    if (E_SUCCESS != ret) {
        LOG_ERROR << "init timer matrix manager failed";
        return ret;
    }
    m_timer_matrix_manager->start();
    LOG_INFO << "init timer matrix manager successfully";

    // cmd_request_service
    LOG_INFO << "begin to init cmd_request_service";
    ret = cmd_request_service::instance().init(vm);
    if (E_SUCCESS != ret) {
        LOG_ERROR << "init cmd_request_service failed";
        return ret;
    }
    cmd_request_service::instance().start();
    LOG_INFO << "init cmd_request_service successfully";

    // node_request_service
    LOG_INFO << "begin to init node_request_service";
    ret = node_request_service::instance().init(vm);
    if (E_SUCCESS != ret) {
        LOG_ERROR << "init node_request_service failed";
        return ret;
    }
    node_request_service::instance().start();
    LOG_INFO << "init node_request_service successfully";

    // data_query_service
    LOG_INFO << "begin to init data_query_service";
    ret = data_query_service::instance().init(vm);
    if (E_SUCCESS != ret) {
        LOG_ERROR << "init data_query_service failed";
        return ret;
    }
    data_query_service::instance().start();
    LOG_INFO << "init data_query_service successfully";

    // connection_manager
    LOG_INFO << "begin to init connection manager";
    ret = dbc::network::connection_manager::instance().init(vm);
    if (E_SUCCESS != ret) {
        LOG_ERROR << "init connection_manager failed";
        return ret;
    }
    dbc::network::connection_manager::instance().start();
    LOG_INFO << "init connection manager successfully";

    // p2p_net_service
    LOG_INFO << "begin to init p2p_net_service";
    ret = p2p_net_service::instance().init(vm);
    if (E_SUCCESS != ret) {
        LOG_ERROR << "init p2p_net_service failed";
        return ret;
    }
    p2p_net_service::instance().start();
    LOG_INFO << "init p2p_net_service successfully";

    LOG_INFO << "begin to init rest_api_service";
    ret = rest_api_service::instance().init(vm);
    if (E_SUCCESS != ret) {
        LOG_ERROR << "init rest_api_service failed";
        return ret;
    }
    rest_api_service::instance().start();
    LOG_INFO << "init rest_api_service successfully";

    LOG_INFO << "begin to init http_server_service";
    ret = http_server_service::instance().init(vm);
    if (E_SUCCESS != ret) {
        LOG_ERROR << "init http_server_service failed";
        return ret;
    }
    http_server_service::instance().start();
    LOG_INFO << "init http_server_service successfully";

    // cost time
    struct timeval t_end{};
    gettimeofday(&t_end, nullptr);
    int64_t cost_time = (t_end.tv_sec - t_start.tv_sec) * 1000 + (t_end.tv_usec - t_start.tv_usec) / 1000;
    LOG_INFO << "server init successfully, cost time: " << cost_time << " ms";

    return E_SUCCESS;
}

void server::idle() {
    while (!m_stop) {
        std::this_thread::sleep_for(std::chrono::milliseconds(DEFAULT_SLEEP_MILLI_SECONDS));
    }
}

void server::exit() {
    m_stop = true;

    m_timer_matrix_manager->stop();
    std::this_thread::sleep_for(std::chrono::milliseconds(DEFAULT_SLEEP_MILLI_SECONDS));
    m_timer_matrix_manager->exit();

    cmd_request_service::instance().stop();
    std::this_thread::sleep_for(std::chrono::milliseconds(DEFAULT_SLEEP_MILLI_SECONDS));
    cmd_request_service::instance().exit();

    node_request_service::instance().stop();
    std::this_thread::sleep_for(std::chrono::milliseconds(DEFAULT_SLEEP_MILLI_SECONDS));
    node_request_service::instance().exit();

    data_query_service::instance().stop();
    std::this_thread::sleep_for(std::chrono::milliseconds(DEFAULT_SLEEP_MILLI_SECONDS));
    data_query_service::instance().exit();

    dbc::network::connection_manager::instance().stop();
    std::this_thread::sleep_for(std::chrono::milliseconds(DEFAULT_SLEEP_MILLI_SECONDS));
    dbc::network::connection_manager::instance().exit();

    p2p_net_service::instance().stop();
    std::this_thread::sleep_for(std::chrono::milliseconds(DEFAULT_SLEEP_MILLI_SECONDS));
    p2p_net_service::instance().exit();

    rest_api_service::instance().stop();
    std::this_thread::sleep_for(std::chrono::milliseconds(DEFAULT_SLEEP_MILLI_SECONDS));
    rest_api_service::instance().exit();

    http_server_service::instance().stop();
    std::this_thread::sleep_for(std::chrono::milliseconds(DEFAULT_SLEEP_MILLI_SECONDS));
    http_server_service::instance().exit();
}

int32_t server::parse_command_line(int argc, const char *const argv[],
                                                 boost::program_options::variables_map &vm) {
    options_description opts("dbc command options");
    opts.add_options()
    ("help,h", "get help info")
    ("version,v", "get version info")
    ("id", "get local node id")
    ("daemon,d", "run as daemon process")
    ("ai_training,a", "run as ai training service provider")
    ("name,n", bpo::value<std::string>(), "node name");

    try {
        bpo::store(bpo::parse_command_line(argc, argv, opts), vm);
        bpo::notify(vm);

        if (vm.count("help") || vm.count("h")) {
            std::cout << opts;
            return E_EXIT_PARSE_COMMAND_LINE;
        } else if (vm.count("version") || vm.count("v")) {
            std::string ver = STR_VER(CORE_VERSION);
            std::cout << ver.substr(2, 2) << "." << ver.substr(4, 2) << "." << ver.substr(6, 2) << "."
            << ver.substr(8, 2);
            std::cout << "\n";
            return E_EXIT_PARSE_COMMAND_LINE;
        } else if (vm.count("daemon") || vm.count("d")) {
            return on_daemon();
        } else if (vm.count("id")) {
            bpo::variables_map vm;

            ERR_CODE ret = env_manager::instance().init(vm);
            if (E_SUCCESS != ret) {
                return ret;
            }

            ret = conf_manager::instance().init(vm);
            if (E_SUCCESS != ret) {
                return ret;
            }

            std::cout << conf_manager::instance().get_node_id();
            std::cout << "\n";
            return E_EXIT_PARSE_COMMAND_LINE;
        } else {
            return E_SUCCESS;
        }
    }
    catch (const std::exception &e) {
        std::cout << "invalid command option " << e.what() << endl;
        std::cout << opts;
    }
    catch (...) {
        std::cout << argv[0] << " invalid command option" << endl;
        std::cout << opts;
    }

    return E_BAD_PARAM;
}

int32_t server::on_cmd_init() {
    //node info
    util::machine_node_info info;
    int32_t ret = util::create_node_info(info);  //check: if exists, not init again and print prompt.
    if (E_SUCCESS != ret) {
        std::cout << "dbc init node info error" << endl;
        LOG_ERROR << "dbc_server_initiator init node info error";
        return ret;
    }

    //serialization
    ret = conf_manager::serialize_node_info(info);
    if (E_SUCCESS != ret) {
        std::cout << "dbc node info serialization failed." << endl;
        LOG_ERROR << "dbc node info serialization failed: node_id=" << info.node_id;
        return ret;
    }

    std::cout << "node id: " << info.node_id << endl;
    LOG_DEBUG << "dbc_server_initiator init node info successfully, node_id: " << info.node_id;

    return E_EXIT_PARSE_COMMAND_LINE;
}

int32_t server::on_daemon() {
#if defined(__linux__) || defined(MAC_OSX)
    if (daemon(1, 1))               //log fd is reserved
        {
        LOG_ERROR << "dbc daemon error: " << strerror(errno);
        return E_DEFAULT;
        }

        LOG_DEBUG << "dbc daemon running succefully";

    //redirect std io to /dev/null
    int fd = open("/dev/null", O_RDWR);
    if (fd < 0) {
        LOG_ERROR << "dbc daemon open /dev/null error";
        return E_DEFAULT;
    }

    dup2(fd, STDIN_FILENO);
    dup2(fd, STDOUT_FILENO);
    dup2(fd, STDERR_FILENO);

    close(fd);

    m_daemon = true;
    return E_SUCCESS;
#else
    LOG_ERROR << "dbc daemon error:  not support";
    return E_DEFAULT;
#endif
}

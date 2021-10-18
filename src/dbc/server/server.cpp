#include "server.h"
#include <fcntl.h>
#include "util/crypto/byteswap.h"
#include "network/connection_manager.h"
#include "config/env_manager.h"
#include "service_module/topic_manager.h"
#include "util/utils/crypto_service.h"
#include <boost/exception/all.hpp>
#include "service_module/service_name.h"
#include "service/http_request_service/http_server_service.h"
#include "service/http_request_service/rest_api_service.h"
#include "service/node_request_service/node_request_service.h"
#include "service/peer_request_service/p2p_net_service.h"
#include "data/resource/system_info.h"

int32_t server::init(int argc, char *argv[]) {
    LOG_INFO << "begin server init ...";

    int32_t ret = E_SUCCESS;
    variables_map args;

    // crypto service
    LOG_INFO << "begin to init crypto service";
    ret = m_crypto.init(args);
    if (E_SUCCESS != ret) {
        LOG_ERROR << "crypto service init failed";
        return ret;
    }
    LOG_INFO << "init crypto service successful";

    // parse command line
    LOG_INFO << "begin to parse command line";
    ret = parse_command_line(argc, argv, args);
    if (E_SUCCESS != ret) {
        LOG_ERROR << "parse command line failed";
        return ret;
    }
    LOG_INFO << "parse command line successful";

    // env_manager
    LOG_INFO << "begin to init env manager";
    ret = env_manager::instance().init();
    if (E_SUCCESS != ret) {
        LOG_ERROR << "init env manager failed";
        return ret;
    }
    LOG_INFO << "init env manager successful";

    // conf_manager
    LOG_INFO << "begin to init conf manager";
    ret = conf_manager::instance().init();
    if (E_SUCCESS != ret) {
        LOG_ERROR << "init conf manager failed";
        return ret;
    }
    LOG_INFO << "init conf manager successful";

    // system_info
    LOG_INFO << "begin to init SystemInfo";
    SystemInfo::instance().init(args);
    SystemInfo::instance().start();
    LOG_INFO << "init SystemInfo successful";

    // timer_matrix_manager
    LOG_INFO << "begin to init timer matrix manager";
    m_timer_matrix_manager = std::make_shared<timer_matrix_manager>();
    ret = m_timer_matrix_manager->init();
    if (E_SUCCESS != ret) {
        LOG_ERROR << "init timer matrix manager failed";
        return ret;
    }
    m_timer_matrix_manager->start();
    LOG_INFO << "init timer matrix manager successful";

    // node_request_service
    LOG_INFO << "begin to init node_request_service";
    ret = node_request_service::instance().init(args);
    if (E_SUCCESS != ret) {
        LOG_ERROR << "init node_request_service failed";
        return ret;
    }
    node_request_service::instance().start();
    LOG_INFO << "init node_request_service successful";

    // network
    LOG_INFO << "begin to init connection manager";
    ret = dbc::network::connection_manager::instance().init();
    if (E_SUCCESS != ret) {
        LOG_ERROR << "init connection_manager failed";
        return ret;
    }
    dbc::network::connection_manager::instance().start();
    LOG_INFO << "init connection manager successful";

    // p2p_net_service
    LOG_INFO << "begin to init p2p_net_service";
    ret = p2p_net_service::instance().init();
    if (E_SUCCESS != ret) {
        LOG_ERROR << "init p2p_net_service failed";
        return ret;
    }
    p2p_net_service::instance().start();
    LOG_INFO << "init p2p_net_service successful";

    LOG_INFO << "begin to init rest_api_service";
    ret = rest_api_service::instance().init();
    if (E_SUCCESS != ret) {
        LOG_ERROR << "init rest_api_service failed";
        return ret;
    }
    rest_api_service::instance().start();
    LOG_INFO << "init rest_api_service successful";

    LOG_INFO << "begin to init http_server_service";
    ret = http_server_service::instance().init(args);
    if (E_SUCCESS != ret) {
        LOG_ERROR << "init http_server_service failed";
        return ret;
    }
    http_server_service::instance().start();
    LOG_INFO << "init http_server_service successful";

    LOG_INFO << "server init successfully";

    return E_SUCCESS;
}

void server::idle() {
    while (!m_stop) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
}

void server::exit() {
    m_stop = true;

    m_timer_matrix_manager->stop();
    node_request_service::instance().stop();
    dbc::network::connection_manager::instance().stop();
    p2p_net_service::instance().stop();
    rest_api_service::instance().stop();
    http_server_service::instance().stop();
}

int32_t server::parse_command_line(int argc, const char *const argv[],
                                                 boost::program_options::variables_map &vm) {
    options_description opts("dbc command options");
    opts.add_options()
        ("id", "get local node id")
        ("daemon,d", "run as daemon process")
        ("ai_training,a", "run as ai training service provider")
        ("name,n", bpo::value<std::string>(), "node name");

    try {
        bpo::store(bpo::parse_command_line(argc, argv, opts), vm);
        bpo::notify(vm);
    }
    catch (const std::exception &e) {
        std::cout << "invalid command option " << e.what() << std::endl;
        std::cout << opts;
        return E_BAD_PARAM;
    }

    if (vm.count("daemon") || vm.count("d")) {
        return on_daemon();
    } else if (vm.count("id")) {
        bpo::variables_map vm;
        ERR_CODE ret = env_manager::instance().init();
        if (E_SUCCESS != ret) {
            return ret;
        }

        ret = conf_manager::instance().init();
        if (E_SUCCESS != ret) {
            return ret;
        }

        std::cout << conf_manager::instance().get_node_id() << std::endl;
        return E_EXIT_PARSE_COMMAND_LINE;
    } else {
        return E_SUCCESS;
    }
}

int32_t server::on_daemon() {
    if (daemon(1, 1)) {
        LOG_ERROR << "dbc daemon error: " << strerror(errno);
        return E_DEFAULT;
    }

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
}

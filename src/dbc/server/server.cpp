#include "server.h"

#include <fcntl.h>
#include <openssl/conf.h>
#include <openssl/crypto.h>
#include <openssl/rand.h>

#include <boost/exception/all.hpp>

#include "config/env_manager.h"
#include "network/connection_manager.h"
#include "service/http_request_service/http_server_service.h"
#include "service/http_request_service/rest_api_service.h"
#include "service/node_monitor_service/node_monitor_service.h"
#include "service/node_request_service/node_request_service.h"
#include "service/peer_request_service/p2p_lan_service.h"
#include "service/peer_request_service/p2p_net_service.h"
#include "task/HttpDBCChainClient.h"
#include "task/bare_metal/bare_metal_node_manager.h"
#include "task/bare_metal/deeplink_lan_service.h"
#include "task/detail/VxlanManager.h"
#include "task/detail/image/ImageManager.h"
#include "util/crypto/key.h"
#include "util/crypto/random.h"
#include "util/crypto/sha256.h"
#include "util/system_info.h"

static std::unique_ptr<ECCVerifyHandle> g_ecc_verify_handle;
static std::unique_ptr<std::recursive_mutex[]> g_ssl_lock;

void ssl_locking_callback(int mode, int type, const char *file, int line) {
    if (mode & CRYPTO_LOCK) {
        g_ssl_lock[type].lock();
    } else {
        g_ssl_lock[type].unlock();
    }
}

NODE_TYPE Server::NodeType = NODE_TYPE::ComputeNode;
std::string Server::NodeName = "";

ERRCODE Server::Init(int argc, char *argv[]) {
    ERRCODE err = ERR_SUCCESS;

    err = ParseCommandLine(argc, argv);
    if (ERR_SUCCESS != err) {
        return err;
    }

    err = dbclog::instance().init();
    if (ERR_SUCCESS != err) {
        return 0;
    }

    LOG_INFO << "begin server init ...";

    // Crypto
    err = InitCrypto();
    if (ERR_SUCCESS != err) {
        LOG_ERROR << "init crypto failed";
        return err;
    }

    // EnvManager
    LOG_INFO << "begin to init EvnManager";
    err = EnvManager::instance().Init();
    if (ERR_SUCCESS != err) {
        LOG_ERROR << "init EnvManager failed";
        return err;
    }
    LOG_INFO << "init EnvManager success";

    // BareMetalNodeManager
    LOG_INFO << "begin to init BareMetalNodeManager";
    err = BareMetalNodeManager::instance().Init();
    if (ERR_SUCCESS != err) {
        LOG_ERROR << "init BareMetalNodeManager failed";
        return err;
    }
    LOG_INFO << "init BareMetalNodeManager success";

    // ConfManager
    LOG_INFO << "begin to init ConfManager";
    err = ConfManager::instance().Init();
    if (ERR_SUCCESS != err) {
        LOG_ERROR << "init ConfManager failed";
        return err;
    }
    LOG_INFO << "init ConfManager success";

    HttpDBCChainClient::instance().init(
        ConfManager::instance().GetDbcChainDomain());

    // SystemInfo
    LOG_INFO << "begin to init SystemInfo";
    SystemInfo::instance().Init(
        Server::NodeType, g_reserved_physical_cores_per_cpu, g_reserved_memory);
    LOG_INFO << "init SystemInfo success";

    // ImageManager
    LOG_INFO << "begin to start ImageManager";
    ImageManager::instance().init();
    LOG_INFO << "start ImageManager success";

    // timer_matrix_manager
    LOG_INFO << "begin to init timer matrix manager";
    m_timer_matrix_manager = std::make_shared<timer_tick_manager>();
    err = m_timer_matrix_manager->init();
    if (ERR_SUCCESS != err) {
        LOG_ERROR << "init timer matrix manager failed";
        return err;
    }
    LOG_INFO << "init timer matrix manager successful";

    // vxlan network manager
    LOG_INFO << "begin to init vxlan netowrk manager";
    err = VxlanManager::instance().Init();
    if (ERR_SUCCESS != err) {
        LOG_ERROR << "init vxlan network manager failed";
        return err;
    }
    LOG_INFO << "init vxlan network manager successful";

    // deeplink lan service
    LOG_INFO << "begin to init deeplink lan service";
    err = deeplink_lan_service::instance().init();
    if (ERR_SUCCESS != err) {
        LOG_ERROR << "init deeplink lan service failed";
        return err;
    }
    LOG_INFO << "init deeplink lan service successful";

    // node_request_service
    LOG_INFO << "begin to init node_request_service";
    err = node_request_service::instance().init();
    if (ERR_SUCCESS != err) {
        LOG_ERROR << "init node_request_service failed";
        return err;
    }
    LOG_INFO << "init node_request_service successful";

    // network
    LOG_INFO << "begin to init connection manager";
    err = network::connection_manager::instance().init();
    if (ERR_SUCCESS != err) {
        LOG_ERROR << "init connection_manager failed";
        return err;
    }
    LOG_INFO << "init connection manager successful";

    // p2p_net_service
    LOG_INFO << "begin to init p2p_net_service";
    err = p2p_net_service::instance().init();
    if (ERR_SUCCESS != err) {
        LOG_ERROR << "init p2p_net_service failed";
        return err;
    }
    LOG_INFO << "init p2p_net_service successful";

    // p2p_lan_service
    LOG_INFO << "begin to init p2p_lan_service";
    err = p2p_lan_service::instance().init();
    if (ERR_SUCCESS != err) {
        LOG_ERROR << "init p2p_lan_service failed";
        return err;
    }
    LOG_INFO << "init p2p_lan_service successful";

    LOG_INFO << "begin to init rest_api_service";
    err = rest_api_service::instance().init();
    if (ERR_SUCCESS != err) {
        LOG_ERROR << "init rest_api_service failed";
        return err;
    }
    LOG_INFO << "init rest_api_service successful";

    LOG_INFO << "begin to init http_server_service";
    err = http_server_service::instance().init();
    if (ERR_SUCCESS != err) {
        LOG_ERROR << "init http_server_service failed";
        return err;
    }
    LOG_INFO << "init http_server_service successful";

    LOG_INFO << "begin to init node_monitor_service";
    err = node_monitor_service::instance().init();
    if (ERR_SUCCESS != err) {
        LOG_ERROR << "init node_monitor_service failed";
        return err;
    }
    LOG_INFO << "init node_monitor_service successful";

    LOG_INFO << "server init successfully";

    return ERR_SUCCESS;
}

ERRCODE Server::InitCrypto() {
    // openssl multithread lock
    g_ssl_lock.reset(new std::recursive_mutex[CRYPTO_num_locks()]);
    CRYPTO_set_locking_callback(ssl_locking_callback);
    OPENSSL_no_config();

    // rand seed
    RandAddSeed();  // Seed OpenSSL PRNG with performance counter

    // elliptic curve code
    std::string sha256_algo = SHA256AutoDetect();
    RandomInit();
    ECC_Start();
    g_ecc_verify_handle.reset(new ECCVerifyHandle());

    // ecc check
    if (!ECC_InitSanityCheck()) {
        // "Elliptic curve cryptography sanity check failure. Aborting.";
        return E_DEFAULT;
    }

    // random check
    if (!Random_SanityCheck()) {
        // LOG_ERROR << "OS cryptographic RNG sanity check failure. Aborting.";
        return E_DEFAULT;
    }

    return ERR_SUCCESS;
}

ERRCODE Server::ExitCrypto() {
    // rand clean
    RAND_cleanup();  // Securely erase the memory used by the PRNG

    // openssl multithread lock
    CRYPTO_set_locking_callback(
        nullptr);        // Shutdown OpenSSL library multithreading support
    g_ssl_lock.reset();  // Clear the set of locks now to maintain symmetry with
                         // the constructor.

    // ecc release
    g_ecc_verify_handle.reset();
    ECC_Stop();

    return ERR_SUCCESS;
}

ERRCODE Server::ParseCommandLine(int argc, char *argv[]) {
    bpo::variables_map options;
    options_description opts("command options");
    // clang-format off
    opts.add_options()
        ("version,v", "dbc version")
        ("compute", "run as compute node")
        ("client", "run as client node")
        ("seed", "run as seed node")
        ("baremetal", "run as bare metal node")
        ("name,n", bpo::value<std::string>(), "node name")
        ("daemon,d", "run as daemon process");
    // clang-format on

    try {
        bpo::store(bpo::parse_command_line(argc, argv, opts), options);
        bpo::notify(options);
    } catch (const std::exception &e) {
        std::cout << "parse command option error: " << e.what() << std::endl;
        std::cout << opts << std::endl;
        return ERR_ERROR;
    }

    if (options.count("version")) {
        std::cout << "version: " << dbcversion() << std::endl;
        return ERR_ERROR;
    }

    if (options.count("compute")) {
        NodeType = NODE_TYPE::ComputeNode;
    } else if (options.count("client")) {
        NodeType = NODE_TYPE::ClientNode;
    } else if (options.count("seed")) {
        NodeType = NODE_TYPE::SeedNode;
    } else if (options.count("baremetal")) {
        NodeType = NODE_TYPE::BareMetalNode;
    }

    if (options.count("name")) {
        NodeName = options["name"].as<std::string>();
    }

    if (NodeName.empty()) {
        char buf[256] = {0};
        gethostname(buf, 256);
        NodeName = buf;
    }

    if (options.count("daemon")) {
        if (ERR_SUCCESS != Daemon()) return ERR_ERROR;
    }

    return ERR_SUCCESS;
}

ERRCODE Server::Daemon() {
    if (daemon(1, 0)) {
        LOG_ERROR << "dbc daemon error: " << strerror(errno);
        return ERR_ERROR;
    }

    return ERR_SUCCESS;
}

void Server::Idle() {
    while (m_running) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

void Server::Exit() {
    LOG_INFO << "server begin exited ...";

    ImageManager::instance().exit();
    LOG_INFO << "ImageManager exited";
    sleep(3);

    VxlanManager::instance().Exit();
    LOG_INFO << "VxlanManager exited";
    // exit(0);

    if (m_timer_matrix_manager) {
        m_timer_matrix_manager->exit();
    }
    LOG_INFO << "m_timer_matrix_manager exited";

    network::connection_manager::instance().exit();
    LOG_INFO << "connection_manager exited";

    p2p_lan_service::instance().exit();
    LOG_INFO << "p2p_lan_service exited";

    p2p_net_service::instance().exit();
    LOG_INFO << "p2p_net_service exited";

    http_server_service::instance().exit();
    LOG_INFO << "http_server_service exited";

    node_request_service::instance().exit();
    LOG_INFO << "node_request_service exited";

    rest_api_service::instance().exit();
    LOG_INFO << "rest_api_service exited";

    node_monitor_service::instance().exit();
    LOG_INFO << "node_monitor_service exited";

    VmClient::instance().exit();
    LOG_INFO << "VmClient exited";

    SystemInfo::instance().exit();
    LOG_INFO << "SystemInfo exited";

    ExitCrypto();
    LOG_INFO << "Crypto exited";

    m_running = false;
    LOG_INFO << "server exited successfully";
}

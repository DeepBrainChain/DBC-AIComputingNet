#include "server.h"
#include <fcntl.h>
#include <openssl/conf.h>
#include <openssl/rand.h>
#include <openssl/crypto.h>
#include "util/crypto/random.h"
#include "util/crypto/sha256.h"
#include "util/crypto/key.h"
#include "network/connection_manager.h"
#include "config/env_manager.h"
#include "service_module/topic_manager.h"
#include <boost/exception/all.hpp>
#include "service/http_request_service/http_server_service.h"
#include "service/http_request_service/rest_api_service.h"
#include "service/node_monitor_service/node_monitor_service.h"
#include "service/node_request_service/node_request_service.h"
#include "service/peer_request_service/p2p_net_service.h"
#include "util/system_info.h"

static std::unique_ptr<ECCVerifyHandle> g_ecc_verify_handle;
static std::unique_ptr<std::recursive_mutex[]> g_ssl_lock;

void ssl_locking_callback(int mode, int type, const char *file, int line)
{
    if (mode & CRYPTO_LOCK)
    {
        g_ssl_lock[type].lock();
    }
    else
    {
        g_ssl_lock[type].unlock();
    }
}

ERRCODE Server::Init(int argc, char *argv[]) {
    ERRCODE err = ERR_SUCCESS;
    variables_map options;

    err = ParseCommandLine(argc, argv, options);
    if (ERR_SUCCESS != err) {
        return err;
    }

    err = dbclog::instance().init();
    if (ERR_SUCCESS != err) {
        return 0;
    }

    LOG_INFO << "begin server init ...";

    LOG_INFO << "begin to init crypto";
    err = InitCrypto(options);
    if (ERR_SUCCESS != err) {
        LOG_ERROR << "init crypto failed";
        return err;
    }
    LOG_INFO << "init crypto success";

    LOG_INFO << "begin to init env_manager";
    err = EnvManager::instance().Init();
    if (ERR_SUCCESS != err) {
        LOG_ERROR << "init env_manager failed";
        return err;
    }
    LOG_INFO << "init env_manager success";

    LOG_INFO << "begin to init ConfManager";
    err = ConfManager::instance().Init();
    if (ERR_SUCCESS != err) {
        LOG_ERROR << "init ConfManager failed";
        return err;
    }
    LOG_INFO << "init ConfManager success";

    // system_info
    LOG_INFO << "begin to init SystemInfo";
    SystemInfo::instance().init(options, g_reserved_physical_cores_per_cpu, g_reserved_memory);
    SystemInfo::instance().start();
    LOG_INFO << "init SystemInfo successful";

    // timer_matrix_manager
    LOG_INFO << "begin to init timer matrix manager";
    m_timer_matrix_manager = std::make_shared<timer_matrix_manager>();
    err = m_timer_matrix_manager->init();
    if (ERR_SUCCESS != err) {
        LOG_ERROR << "init timer matrix manager failed";
        return err;
    }
    m_timer_matrix_manager->start();
    LOG_INFO << "init timer matrix manager successful";

    // node_request_service
    LOG_INFO << "begin to init node_request_service";
    err = node_request_service::instance().init(options);
    if (ERR_SUCCESS != err) {
        LOG_ERROR << "init node_request_service failed";
        return err;
    }
    node_request_service::instance().start();
    LOG_INFO << "init node_request_service successful";

    // network
    LOG_INFO << "begin to init connection manager";
    err = dbc::network::connection_manager::instance().init();
    if (ERR_SUCCESS != err) {
        LOG_ERROR << "init connection_manager failed";
        return err;
    }
    dbc::network::connection_manager::instance().start();
    LOG_INFO << "init connection manager successful";

    // p2p_net_service
    LOG_INFO << "begin to init p2p_net_service";
    err = p2p_net_service::instance().init();
    if (ERR_SUCCESS != err) {
        LOG_ERROR << "init p2p_net_service failed";
        return err;
    }
    p2p_net_service::instance().start();
    LOG_INFO << "init p2p_net_service successful";

    LOG_INFO << "begin to init rest_api_service";
    err = rest_api_service::instance().init();
    if (ERR_SUCCESS != err) {
        LOG_ERROR << "init rest_api_service failed";
        return err;
    }
    rest_api_service::instance().start();
    LOG_INFO << "init rest_api_service successful";

    LOG_INFO << "begin to init http_server_service";
    err = http_server_service::instance().init(options);
    if (ERR_SUCCESS != err) {
        LOG_ERROR << "init http_server_service failed";
        return err;
    }
    http_server_service::instance().start();
    LOG_INFO << "init http_server_service successful";

    LOG_INFO << "begin to init node_monitor_service";
    err = node_monitor_service::instance().init(options);
    if (ERR_SUCCESS != err) {
        LOG_ERROR << "init node_monitor_service failed";
        return err;
    }
    node_monitor_service::instance().start();
    LOG_INFO << "init node_monitor_service successful";

    LOG_INFO << "server init successfully";

    return ERR_SUCCESS;
}

ERRCODE Server::InitCrypto(bpo::variables_map &options) {
    //openssl multithread lock
    g_ssl_lock.reset(new std::recursive_mutex[CRYPTO_num_locks()]);
    CRYPTO_set_locking_callback(ssl_locking_callback);
    OPENSSL_no_config();

    //rand seed
    RandAddSeed();                          // Seed OpenSSL PRNG with performance counter

    //elliptic curve code
    std::string sha256_algo = SHA256AutoDetect();
    RandomInit();
    ECC_Start();
    g_ecc_verify_handle.reset(new ECCVerifyHandle());

    //ecc check
    if (!ECC_InitSanityCheck())
    {
        // "Elliptic curve cryptography sanity check failure. Aborting.";
        return E_DEFAULT;
    }

    //random check
    if (!Random_SanityCheck())
    {
        //LOG_ERROR << "OS cryptographic RNG sanity check failure. Aborting.";
        return E_DEFAULT;
    }

    return ERR_SUCCESS;
}

ERRCODE Server::ExitCrypto()
{
    //rand clean
    RAND_cleanup();                         // Securely erase the memory used by the PRNG

    //openssl multithread lock
    CRYPTO_set_locking_callback(nullptr);               // Shutdown OpenSSL library multithreading support
    g_ssl_lock.reset();                         // Clear the set of locks now to maintain symmetry with the constructor.

    //ecc release
    g_ecc_verify_handle.reset();
    ECC_Stop();

    return ERR_SUCCESS;
}

ERRCODE Server::ParseCommandLine(int argc, char* argv[], bpo::variables_map &options) {
    options_description opts("dbc command options");
    opts.add_options()
            ("daemon,d", "run as daemon process")
            ("ai_training,a", "run as ai training service provider")
            ("name,n", bpo::value<std::string>(), "node name")
            ("version,v", "dbc version");

    try {
        bpo::store(bpo::parse_command_line(argc, argv, opts), options);
        bpo::notify(options);
    }
    catch (const std::exception &e) {
        std::cout << "invalid command option: " << e.what() << std::endl;
        std::cout << opts << std::endl;
        return ERR_ERROR;
    }

    if (options.count("version")) {
        std::cout << "version: " << dbcversion() << std::endl;
        return ERR_ERROR;
    } else if (options.count("daemon")) {
        Daemon();
        return ERR_SUCCESS;
    } else {
        return ERR_SUCCESS;
    }
}

void Server::Daemon()
{
    pid_t pid;
    if ((pid = fork()) != 0) {
        ::exit(0);
    }

    setsid();
    signal(SIGINT, SIG_IGN);
    signal(SIGHUP, SIG_IGN);
    signal(SIGQUIT, SIG_IGN);
    signal(SIGPIPE, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGCHLD, SIG_IGN);
    signal(SIGTERM, SIG_IGN);

    // ignore pipe
    struct sigaction sig;
    sig.sa_handler = SIG_IGN;
    sig.sa_flags = 0;
    sigemptyset(&sig.sa_mask);
    sigaction(SIGPIPE, &sig, NULL);

    if ((pid = fork()) != 0) 	{
        ::exit(0);
    }

    //chdir("/");

    umask(0);

    for (int i = 3; i < 64; i++)
        close(i);
}

void Server::Idle() {
    while (m_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
}

void Server::Exit() {
    if (m_timer_matrix_manager)
        m_timer_matrix_manager->stop();
    dbc::network::connection_manager::instance().stop();
    p2p_net_service::instance().stop();
    http_server_service::instance().stop();
    node_request_service::instance().stop();
    rest_api_service::instance().stop();
    ExitCrypto();

    m_running = false;
}


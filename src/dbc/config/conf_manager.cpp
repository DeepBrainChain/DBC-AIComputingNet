#include "conf_manager.h"
#include "env_manager.h"
#include <fstream> 
#include <boost/exception/all.hpp>
#include <vector>
#include "util/crypto/utilstrencodings.h"
#include <boost/format.hpp>
#include "network/protocol/thrift_compact.h"
#include "log/log.h"
#include "util/tweetnacl/randombytes.h"
#include "util/tweetnacl/tools.h"
#include "util/tweetnacl/tweetnacl.h"

conf_manager::conf_manager()
{
    m_net_params = std::make_shared<net_type_params>();
    m_proto_capacity.add(dbc::network::matrix_capacity::THRIFT_BINARY_C_NAME);
    m_proto_capacity.add(dbc::network::matrix_capacity::THRIFT_COMPACT_C_NAME);
    m_proto_capacity.add(dbc::network::matrix_capacity::SNAPPY_RAW_C_NAME);
}

ERR_CODE conf_manager::init(bpo::variables_map &options)
{
    ERR_CODE ret = E_SUCCESS;

    // 解析本地配置文件
    ret = parse_local_conf();
    if (E_SUCCESS != ret)
    {
        LOG_ERROR << "conf manager parse local conf error and exit";
        return ret;
    }

    // 生成机器id，写入 node.dat
    ret = parse_node_dat();
    if (E_SUCCESS != ret)
    {
        LOG_ERROR << "conf manager parse node dat error and exit";
        return ret;
    }

    // init params
    ret = init_params();
    if (E_SUCCESS != ret)
    {
        LOG_ERROR << "conf manager init params error and exit";
        return ret;
    }

    //update log level
    log::set_filter_level((boost::log::trivial::severity_level) m_log_level);

    return E_SUCCESS;
}

int32_t conf_manager::parse_local_conf()
{
    // core.conf
    bpo::options_description core_opts("core.conf options");
    core_opts.add_options()
        ("log_level", bpo::value<uint32_t>()->default_value(DEFAULT_LOG_LEVEL))
        ("net_type", bpo::value<std::string>()->default_value(""), "")
        ("host_ip", bpo::value<std::string>()->default_value(DEFAULT_LOCAL_IP), "")
        ("main_net_listen_port", bpo::value<std::string>()->default_value(DEFAULT_MAIN_NET_LISTEN_PORT), "")
        ("max_connect", bpo::value<int32_t>()->default_value(128), "")
        ("timer_service_broadcast_in_second", bpo::value<int32_t>()->default_value(DEFAULT_TIMER_SERVICE_BROADCAST_IN_SECOND), "")
        ("timer_dbc_request_in_second", bpo::value<int32_t>()->default_value(DEFAULT_TIMER_DBC_REQUEST_IN_SECOND), "")
        ("timer_ai_training_task_schedule_in_second", bpo::value<int32_t>()->default_value(DEFAULT_TIMER_AI_TRAINING_TASK_SCHEDULE_IN_SECOND), "")
        ("timer_log_refresh_in_second", bpo::value<int32_t>()->default_value(DEFAULT_TIMER_LOG_REFRESH_IN_SECOND), "")
        ("magic_num", bpo::value<std::string>()->default_value("0XE1D1A098"), "")
        ("max_recv_speed", bpo::value<int32_t>()->default_value(0), "")
        ("update_idle_task_cycle", bpo::value<int32_t>()->default_value(DEFAULT_UPDATE_IDLE_TASK_CYCLE), "")
        ("timer_service_list_expired_in_second", bpo::value<int32_t>()->default_value(DEFAULT_TIMER_SERVICE_LIST_EXPIRED_IN_SECOND), "")
        ("rest_ip", bpo::value<std::string>()->default_value(DEFAULT_LOOPBACK_IP), "http server ip address")
        ("rest_port", bpo::value<std::string>()->default_value(DEFAULT_REST_PORT), "0 prohibit http server")
        ("prune_container_stop_interval", bpo::value<int16_t>()->default_value(DEFAULT_PRUNE_CONTAINER_INTERVAL), "")
        ("prune_docker_root_use_ratio", bpo::value<int16_t>()->default_value(DEFALUT_PRUNE_DOCKER_ROOT_USE_RATIO), "")
        ("prune_task_stop_interval", bpo::value<int16_t>()->default_value(DEFAULT_PRUNE_TASK_INTERVAL), "")
        ("dbc_chain_domain", bpo::value<std::string>()->default_value(""), "");

    const boost::filesystem::path &conf_path = env_manager::instance().get_conf_path();
    try {
        std::ifstream conf_ifs(conf_path.generic_string());
        bpo::store(bpo::parse_config_file(conf_ifs, core_opts), m_args);
        bpo::notify(m_args);
    }
    catch (const boost::exception & e)
    {
        LOG_ERROR << "core.conf parse error: " << diagnostic_information(e);
        return E_DEFAULT;
    }

    // peer.conf
    bpo::options_description peer_opts("peer.conf options");
    peer_opts.add_options()
        ("peer", bpo::value<std::vector<std::string>>(), "");

    const boost::filesystem::path &peer_path = env_manager::instance().get_peer_path();
    try
    {
        std::ifstream peer_ifs(peer_path.generic_string());
        bpo::store(bpo::parse_config_file(peer_ifs, peer_opts), m_args);
        bpo::notify(m_args);
    }
    catch (const boost::exception & e)
    {
        LOG_ERROR << "peer.conf parse error: " << diagnostic_information(e);
        return E_DEFAULT;
    }

    return E_SUCCESS;
}

int32_t conf_manager::serialize_node_info(const util::machine_node_info &info)
{
    FILE *fp = nullptr;
    boost::filesystem::path node_dat_path;

    try
    {
        node_dat_path /= util::get_exe_dir();
        node_dat_path /= boost::filesystem::path(DAT_DIR_NAME);

        if (!boost::filesystem::exists(node_dat_path))
        {
            LOG_DEBUG << "dat directory path does not exist and create dat directory";
            boost::filesystem::create_directory(node_dat_path);
        }

        if (!boost::filesystem::is_directory(node_dat_path))
        {
            LOG_ERROR << "dat directory path does not exist and exit";
            return E_DEFAULT;
        }

        node_dat_path /= boost::filesystem::path(NODE_FILE_NAME);

        //open file w+
#ifdef WIN32
        errno_t err = fopen_s(&fp, node_dat_path.generic_string().c_str(), "w+");
        if (0 != err)
#else
        fp = fopen(node_dat_path.generic_string().c_str(), "w+");
        if (nullptr == fp)
#endif
        {
            LOG_ERROR << "conf_manager open node.dat error: fp is nullptr";
            return E_DEFAULT;
        }
    }
    catch (const std::exception & e)
    {
        LOG_ERROR << "create node error: " << e.what();
        return E_DEFAULT;
    }
    catch (const boost::exception & e)
    {
        LOG_ERROR << "create node error" << diagnostic_information(e);
        return E_DEFAULT;
    }

    assert(nullptr != fp);

    fprintf(fp, "node_id=");
    fprintf(fp, "%s",info.node_id.c_str());
    fprintf(fp, "\n");

    fprintf(fp, "node_private_key=");
    fprintf(fp, "%s",info.node_private_key.c_str());
    fprintf(fp, "\n");

    if (fp)
    {
        fclose(fp);
    }
    return E_SUCCESS;
}

int32_t conf_manager::gen_new_nodeid()
{
    //node info
    util::machine_node_info info;
    int32_t ret = util::create_node_info(info);                 //check: if exists, not init again and print promption.
    if (E_SUCCESS != ret)
    {
        LOG_ERROR << "dbc_server_initiator init node info error";
        return ret;
    }

    //serialization
    ret = conf_manager::serialize_node_info(info);
    if (E_SUCCESS != ret)
    {
        LOG_ERROR << "dbc node info serialization failed: node_id=" << info.node_id;
        return ret;
    }
    LOG_DEBUG << "dbc_server_initiator init node info successfully, node_id: " << info.node_id;
    return E_SUCCESS;
}

int32_t conf_manager::parse_node_dat()
{
    boost::filesystem::path node_dat_path = env_manager::instance().get_dat_path();
    node_dat_path /= boost::filesystem::path(NODE_FILE_NAME);

    if (!boost::filesystem::exists(node_dat_path) || boost::filesystem::is_empty(node_dat_path))
    {
        int32_t gen_ret = gen_new_nodeid();
        if (gen_ret != E_SUCCESS)
        {
            LOG_ERROR << "generate new nodeid error";
            return gen_ret;
        }
    }

    {
        FILE *fp = fopen(node_dat_path.generic_string().c_str(), "a+");
        if (nullptr == fp) {
            LOG_ERROR << "fp is nullptr";
            return E_DEFAULT;
        }
        fseek(fp, 0, SEEK_SET);
        char tmpbuf[2048] = {0};
        fread(tmpbuf, 1, 2000, fp);

        std::string sbuf(tmpbuf);
        if (sbuf.find("pub_key=") == std::string::npos) {
            unsigned char pub_key[32];
            unsigned char priv_key[32];
            crypto_box_keypair(pub_key, priv_key);
            char* p_pub = bytes_to_hex(pub_key, 32);
            char* p_priv = bytes_to_hex(priv_key, 32);

            fprintf(fp, "\n");

            fprintf(fp, "pub_key=");
            fprintf(fp, "%s", p_pub);
            fprintf(fp, "\n");

            fprintf(fp, "priv_key=");
            fprintf(fp, "%s", p_priv);
            fprintf(fp, "\n");

            free(p_pub);
            free(p_priv);
        }

        fclose(fp);
    }

    try
    {
        bpo::options_description node_dat_opts("node.dat options");
        node_dat_opts.add_options()
            ("node_id", bpo::value<std::string>(), "")
            ("node_private_key", bpo::value<std::string>(), "")
            ("pub_key", bpo::value<std::string>(), "")
            ("priv_key", bpo::value<std::string>(), "");

        std::ifstream node_dat_ifs(node_dat_path.generic_string());
        bpo::store(bpo::parse_config_file(node_dat_ifs, node_dat_opts), m_args);
        bpo::notify(m_args);
    }
    catch (const boost::exception & e)
    {
        LOG_ERROR << "conf manager parse node.dat error: " << diagnostic_information(e);
        return E_DEFAULT;
    }

    return E_SUCCESS;
}

bool conf_manager::check_node_info()
{
    std::string node_derived = util::derive_pubkey_by_privkey(m_node_private_key);
    return node_derived == m_node_id;
}

int32_t conf_manager::init_params()
{
    if (0 == m_args.count("node_id"))
    {
        LOG_ERROR << "conf_manager has no node id error";
        return E_DEFAULT;
    }
    m_node_id = SanitizeString(m_args["node_id"].as<std::string>());

    if (0 == m_args.count("node_private_key"))
    {
        LOG_ERROR << "conf_manager has no node private key error";
        return E_DEFAULT;
    }
    m_node_private_key = SanitizeString(m_args["node_private_key"].as<std::string>());

    if (!check_node_info())
    {
        LOG_ERROR << "node_id error";
        return E_DEFAULT;
    }

    m_net_type = MAIN_NET_TYPE;
    m_net_flag = MAIN_NET;

    // p2pnet
    const std::string & net_listen_port = m_args.count("main_net_listen_port") ? m_args["main_net_listen_port"].as<std::string>() : DEFAULT_MAIN_NET_LISTEN_PORT;
    m_net_params->init_net_listen_port(net_listen_port);
    m_net_params->init_seeds();

    //init log level
    if (0 != m_args.count("log_level"))
    {
        m_log_level = m_args["log_level"].as<uint32_t>();
    }

    if (0 != m_args.count("pub_key")) {
        m_pub_key = m_args["pub_key"].as<std::string>();
    }

    if (0 != m_args.count("priv_key")) {
        m_priv_key = m_args["priv_key"].as<std::string>();
    }

    return E_SUCCESS;
}

uint16_t conf_manager::get_net_default_port()
{
    return (uint16_t)std::stoi(DEFAULT_MAIN_NET_LISTEN_PORT);
}

#include "conf_manager.h"
#include "env_manager.h"
#include <fstream> 
#include <boost/exception/all.hpp>
#include <vector>
#include "util/crypto/utilstrencodings.h"
#include <boost/format.hpp>
#include "network/protocol/thrift_compact.h"
#include "log/log.h"
#include "tweetnacl/randombytes.h"
#include "tweetnacl/tools.h"
#include "tweetnacl/tweetnacl.h"

static std::string g_internal_ip_seeds[] = {
        "115.231.234.43:5001",
        "115.231.234.43:5003",
        "115.231.234.43:5005",
        "115.231.234.43:5007",
        "115.231.234.43:5009",

        "115.231.234.32:5001",
        "115.231.234.32:5003",
        "115.231.234.32:5005",
        "115.231.234.32:5007",
        "115.231.234.32:5009",

        "115.231.234.40:5001",
        "115.231.234.40:5003",
        "115.231.234.40:5005",
        "115.231.234.40:5007",
        "115.231.234.40:5009",

        "115.231.234.34:5001",
        "115.231.234.34:5003",
        "115.231.234.34:5005",
        "115.231.234.34:5007",
        "115.231.234.34:5009",

        "115.231.234.35:5001",
        "115.231.234.35:5003",
        "115.231.234.35:5005",
        "115.231.234.35:5007",
        "115.231.234.35:5009",

        "115.231.234.41:5001",
        "115.231.234.41:5003",
        "115.231.234.41:5005",
        "115.231.234.41:5007",
        "115.231.234.41:5009",

        "115.231.234.37:5001",
        "115.231.234.37:5003",
        "115.231.234.37:5005",
        "115.231.234.37:5007",
        "115.231.234.37:5009"
};

static std::string g_internal_dns_seeds[] = {

};

conf_manager::conf_manager() {
    m_proto_capacity.add(dbc::network::matrix_capacity::THRIFT_BINARY_C_NAME);
    m_proto_capacity.add(dbc::network::matrix_capacity::THRIFT_COMPACT_C_NAME);
    m_proto_capacity.add(dbc::network::matrix_capacity::SNAPPY_RAW_C_NAME);
}

ERR_CODE conf_manager::init() {
    ERR_CODE ret = E_SUCCESS;

    // 解析配置文件
    ret = parse_local_conf();
    if (E_SUCCESS != ret)
    {
        LOG_ERROR << "conf manager parse local conf error and exit";
        return ret;
    }

    // 解析node.dat
    ret = parse_node_dat();
    if (E_SUCCESS != ret)
    {
        LOG_ERROR << "conf manager parse node dat error and exit";
        return ret;
    }

    // 创建加密通信密钥对
    create_crypto_keypair();

    dbclog::instance().set_filter_level((boost::log::trivial::severity_level) m_log_level);

    return E_SUCCESS;
}

int32_t conf_manager::parse_local_conf() {
    // core.conf
    variables_map core_args;
    bpo::options_description core_opts("core.conf options");
    core_opts.add_options()
        ("log_level", bpo::value<int32_t>()->default_value(2), "")
        ("net_type", bpo::value<std::string>()->default_value("mainnet"), "")
        ("net_flag", bpo::value<std::string>()->default_value("0xF1E1B0F9"), "")
        ("net_listen_ip", bpo::value<std::string>()->default_value("0.0.0.0"), "")
        ("net_listen_port", bpo::value<int32_t>()->default_value(10001), "")
        ("max_connect_count", bpo::value<int32_t>()->default_value(1024), "")
        ("http_ip", bpo::value<std::string>()->default_value("127.0.0.1"), "")
        ("http_port", bpo::value<int32_t>()->default_value(20001), "")
        ("dbc_chain_domain", bpo::value<std::vector<std::string>>(), "")
        ("version", bpo::value<std::string>()->default_value("0.3.7.3"), "");

    const boost::filesystem::path &conf_path = env_manager::instance().get_conf_path();
    try {
        std::ifstream conf_ifs(conf_path.generic_string());
        bpo::store(bpo::parse_config_file(conf_ifs, core_opts), core_args);
        bpo::notify(core_args);
    }
    catch (const boost::exception & e)
    {
        LOG_ERROR << "core.conf parse error: " << diagnostic_information(e);
        return E_DEFAULT;
    }

    m_log_level = core_args["log_level"].as<int32_t>();

    m_net_type = core_args["net_type"].as<std::string>();
    m_net_flag = strtoul(core_args["net_flag"].as<std::string>().c_str(), nullptr, 16);
    m_net_listen_ip = core_args["net_listen_ip"].as<std::string>();
    m_net_listen_port = core_args["net_listen_port"].as<int32_t>();
    m_max_connect_count = core_args["max_connect_count"].as<int32_t>();

    m_http_ip = core_args["http_ip"].as<std::string>();
    m_http_port = core_args["http_port"].as<int32_t>();

    if (core_args.count("dbc_chain_domain") > 0) {
        m_dbc_chain_domain = core_args["dbc_chain_domain"].as<std::vector<std::string>>();
    }

    m_version = core_args["version"].as<std::string>();

    // peer.conf
    variables_map peer_args;
    bpo::options_description peer_opts("peer.conf options");
    peer_opts.add_options()
        ("peer", bpo::value<std::vector<std::string>>(), "");

    const boost::filesystem::path &peer_path = env_manager::instance().get_peer_path();
    try {
        std::ifstream peer_ifs(peer_path.generic_string());
        bpo::store(bpo::parse_config_file(peer_ifs, peer_opts), peer_args);
        bpo::notify(peer_args);
    }
    catch (const boost::exception & e)
    {
        LOG_ERROR << "peer.conf parse error: " << diagnostic_information(e);
        return E_DEFAULT;
    }

    if (peer_args.count("peer") > 0)
        m_peers = peer_args["peer"].as<std::vector<std::string>>();

    int count = sizeof(g_internal_ip_seeds) / sizeof(std::string);
    for (int i = 0; i < count; i++) {
        m_internal_ip_seeds.push_back(g_internal_ip_seeds[i]);
    }

    count = sizeof(g_internal_dns_seeds) / sizeof(std::string);
    for (int i = 0; i < count; i++) {
        m_internal_dns_seeds.push_back(g_internal_dns_seeds[i]);
    }

    return E_SUCCESS;
}

int32_t conf_manager::parse_node_dat() {
    boost::filesystem::path node_dat_path = env_manager::instance().get_dat_path();
    node_dat_path /= boost::filesystem::path("node.dat");

    if (!boost::filesystem::exists(node_dat_path) || boost::filesystem::is_empty(node_dat_path)) {
        int32_t gen_ret = create_new_nodeid();
        if (gen_ret != E_SUCCESS) {
            LOG_ERROR << "create new nodeid error";
            return gen_ret;
        }
    }

    variables_map node_dat_args;
    bpo::options_description node_dat_opts("node.dat options");
    node_dat_opts.add_options()
        ("node_id", bpo::value<std::string>(), "")
        ("node_private_key", bpo::value<std::string>(), "")
        ("pub_key", bpo::value<std::string>(), "")
        ("priv_key", bpo::value<std::string>(), "");

    try
    {
        std::ifstream node_dat_ifs(node_dat_path.generic_string());
        bpo::store(bpo::parse_config_file(node_dat_ifs, node_dat_opts), node_dat_args);
        bpo::notify(node_dat_args);
    }
    catch (const boost::exception & e)
    {
        LOG_ERROR << "parse node.dat error: " << diagnostic_information(e);
        return E_DEFAULT;
    }

    m_node_id = node_dat_args["node_id"].as<std::string>();
    m_node_private_key = node_dat_args["node_private_key"].as<std::string>();

    if (!check_node_id()) {
        LOG_ERROR << "node_id error";
        return E_DEFAULT;
    }

    return E_SUCCESS;
}

ERR_CODE conf_manager::create_new_nodeid() {
    util::machine_node_info info;
    int32_t ret = util::create_node_info(info);
    if (E_SUCCESS != ret) {
        LOG_ERROR << "dbc_server_initiator init node info error";
        return ret;
    }

    ret = conf_manager::serialize_node_info(info);
    if (E_SUCCESS != ret) {
        LOG_ERROR << "dbc node info serialization failed: node_id=" << info.node_id;
        return ret;
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

        node_dat_path /= boost::filesystem::path("node.dat");

        //open file w+
        fp = fopen(node_dat_path.generic_string().c_str(), "w+");
        if (nullptr == fp)
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

    if (fp) {
        fclose(fp);
    }

    return E_SUCCESS;
}

bool conf_manager::check_node_id()
{
    std::string node_derived = util::derive_pubkey_by_privkey(m_node_private_key);
    return node_derived == m_node_id;
}

void conf_manager::create_crypto_keypair() {
    unsigned char pub_key[32];
    unsigned char priv_key[32];
    crypto_box_keypair(pub_key, priv_key);
    char* p_pub = bytes_to_hex(pub_key, 32);
    char* p_priv = bytes_to_hex(priv_key, 32);
    m_pub_key = p_pub;
    m_priv_key = p_priv;
    free(p_pub);
    free(p_priv);
}

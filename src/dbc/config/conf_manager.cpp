#include "conf_manager.h"
#include "env_manager.h"
#include <boost/exception/all.hpp>
#include <boost/format.hpp>
#include "util/crypto/utilstrencodings.h"
#include "log/log.h"
#include "tweetnacl/tools.h"
#include "tweetnacl/tweetnacl.h"
#include "BareMetalNodeManager.h"
#include "server/server.h"

static std::string g_internal_dns_seeds[] = {

};

static std::string g_internal_ip_seeds[] = {
        "8.214.55.62:5001",
        "8.214.55.62:5011",
        "8.214.55.62:5021",
        "8.214.55.62:5031",

        "116.169.53.130:5001",
        "116.169.53.130:5011",
        "116.169.53.130:5021",
        "116.169.53.130:5031",

        "116.169.53.131:5001",
        "116.169.53.131:5011",
        "116.169.53.131:5021",
        "116.169.53.131:5031"
};

ConfManager::ConfManager() {
    m_proto_capacity.add(network::matrix_capacity::THRIFT_BINARY_C_NAME);
    m_proto_capacity.add(network::matrix_capacity::THRIFT_COMPACT_C_NAME);
    m_proto_capacity.add(network::matrix_capacity::SNAPPY_RAW_C_NAME);
}

ConfManager::~ConfManager() {
    for (auto& it : m_image_server) {
        delete it.second;
    }
}

ERRCODE ConfManager::Init() {
    ERRCODE err;
    err = ParseConf();
    if (ERR_SUCCESS != err)
    {
        LOG_ERROR << "parse conf failed";
        return err;
    }

    err = ParseDat();
    if (ERR_SUCCESS != err)
    {
        LOG_ERROR << "parse dat failed";
        return err;
    }

    CreateCryptoKeypair();

    dbclog::instance().set_filter_level((boost::log::trivial::severity_level) m_log_level);

    return ERR_SUCCESS;
}

std::string ConfManager::GetNodePrivateKey(const std::string& node_id) const {
    if (node_id.empty() || node_id == m_node_id)
        return m_node_private_key;
    if (Server::NodeType == NODE_TYPE::BareMetalNode) {
        typedef std::map<std::string, std::shared_ptr<dbc::db_bare_metal>> BM_NODES;
        const BM_NODES bare_metal_nodes =
            BareMetalNodeManager::instance().getBareMetalNodes();
        // if (bare_metal_nodes.count(node_id) > 0)
        //     return bare_metal_nodes[node_id]->node_private_key;
        BM_NODES::const_iterator iter = bare_metal_nodes.find(node_id);
        if (iter != bare_metal_nodes.end())
            return iter->second->node_private_key;
    }
    return m_node_private_key;
}

ERRCODE ConfManager::ParseConf() {
    variables_map core_args;
    bpo::options_description core_opts("core.conf");
    core_opts.add_options()
        ("version", bpo::value<std::string>()->default_value("0.3.7.3"), "")
        ("log_level", bpo::value<int32_t>()->default_value(2), "")
        ("net_type", bpo::value<std::string>()->default_value("mainnet"), "")
        ("net_flag", bpo::value<std::string>()->default_value("0xF1E1B0F9"), "")
        ("net_listen_ip", bpo::value<std::string>()->default_value("0.0.0.0"), "")
        ("net_listen_port", bpo::value<int32_t>()->default_value(5001), "")
        ("max_connect_count", bpo::value<int32_t>()->default_value(1024), "")
        ("http_ip", bpo::value<std::string>()->default_value("127.0.0.1"), "")
        ("http_port", bpo::value<int32_t>()->default_value(5050), "")
        ("dbc_chain_domain", bpo::value<std::vector<std::string>>(), "")
        ("image_server", bpo::value<std::vector<std::string>>(), "")
        ("dbc_monitor_server", bpo::value<std::string>()->default_value("monitor.dbcwallet.io:10051"), "")
        ("miner_monitor_server", bpo::value<std::string>(), "")
        ("multicast_address", bpo::value<std::string>()->default_value("239.255.0.1"), "")
        ("multicast_port", bpo::value<int32_t>()->default_value(30001), "");

    const boost::filesystem::path &conf_path = EnvManager::instance().get_conf_file_path();
    try {
        std::ifstream conf_ifs(conf_path.generic_string());
        bpo::store(bpo::parse_config_file(conf_ifs, core_opts), core_args);
        bpo::notify(core_args);
    }
    catch (const boost::exception & e)
    {
        LOG_ERROR << "core.conf parse error: " << diagnostic_information(e);
        return ERR_ERROR;
    }

    m_version = core_args["version"].as<std::string>();
    m_log_level = core_args["log_level"].as<int32_t>();
    m_net_type = core_args["net_type"].as<std::string>();
    m_net_flag = strtoul(core_args["net_flag"].as<std::string>().c_str(), nullptr, 16);
    m_net_listen_ip = core_args["net_listen_ip"].as<std::string>();
    m_net_listen_port = core_args["net_listen_port"].as<int32_t>();
    m_max_connect_count = core_args["max_connect_count"].as<int32_t>();
    m_http_listen_ip = core_args["http_ip"].as<std::string>();
    m_http_listen_port = core_args["http_port"].as<int32_t>();

    if (core_args.count("dbc_chain_domain") > 0) {
        m_dbc_chain_domain = core_args["dbc_chain_domain"].as<std::vector<std::string>>();
    }

    if (core_args.count("image_server") > 0) {
        std::vector<std::string> vec = core_args["image_server"].as<std::vector<std::string>>();
        for (auto& str : vec) {
            ImageServer* imgsvr = new ImageServer();
            imgsvr->from_string(str);
            if (imgsvr->id.empty()) {
                delete imgsvr;
                continue;
            }
			m_image_server[imgsvr->id] = imgsvr;
        }
    }

    if (core_args.count("dbc_monitor_server") > 0) {
        m_dbc_monitor_server = core_args["dbc_monitor_server"].as<std::string>();
    }

    if (core_args.count("miner_monitor_server") > 0) {
        m_miner_monitor_server = core_args["miner_monitor_server"].as<std::string>();
    }

    m_multicast_address = core_args["multicast_address"].as<std::string>();
    m_multicast_port = core_args["multicast_port"].as<int32_t>();

    variables_map peer_args;
    bpo::options_description peer_opts("peer.conf");
    peer_opts.add_options()
        ("peer", bpo::value<std::vector<std::string>>(), "");

    const boost::filesystem::path &peer_path = EnvManager::instance().get_peer_file_path();
    try {
        std::ifstream peer_ifs(peer_path.generic_string());
        bpo::store(bpo::parse_config_file(peer_ifs, peer_opts), peer_args);
        bpo::notify(peer_args);
    }
    catch (const boost::exception & e)
    {
        LOG_ERROR << "peer.conf parse error: " << diagnostic_information(e);
        return ERR_ERROR;
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

    return ERR_SUCCESS;
}

ERRCODE ConfManager::ParseDat() {
    boost::filesystem::path node_dat_file_path = EnvManager::instance().get_dat_file_path();
    if (!boost::filesystem::exists(node_dat_file_path) || boost::filesystem::is_empty(node_dat_file_path)) {
        int32_t gen_ret = CreateNewNodeId();
        if (gen_ret != ERR_SUCCESS) {
            LOG_ERROR << "create new nodeid error";
            return gen_ret;
        }
    }

    variables_map node_dat_args;
    bpo::options_description node_dat_opts("node.dat");
    node_dat_opts.add_options()
        ("node_id", bpo::value<std::string>(), "")
        ("node_private_key", bpo::value<std::string>(), "")
        ("pub_key", bpo::value<std::string>(), "")
        ("priv_key", bpo::value<std::string>(), "");

    try
    {
        std::ifstream node_dat_ifs(node_dat_file_path.generic_string());
        bpo::store(bpo::parse_config_file(node_dat_ifs, node_dat_opts), node_dat_args);
        bpo::notify(node_dat_args);
    }
    catch (const boost::exception & e)
    {
        LOG_ERROR << "parse node.dat error: " << diagnostic_information(e);
        return ERR_ERROR;
    }

    m_node_id = node_dat_args["node_id"].as<std::string>();
    m_node_private_key = node_dat_args["node_private_key"].as<std::string>();

    if (!CheckNodeId()) {
        LOG_ERROR << "node_id error";
        return ERR_ERROR;
    }

    return ERR_SUCCESS;
}

ERRCODE ConfManager::CreateNewNodeId() {
    util::machine_node_info info;
    int32_t ret = util::create_node_info(info);
    if (0 != ret) {
        LOG_ERROR << "dbc_server_initiator init node info error";
        return ret;
    }

    ret = SerializeNodeInfo(info);
    if (ERR_SUCCESS != ret) {
        LOG_ERROR << "dbc node info serialization failed: node_id=" << info.node_id;
        return ret;
    }

    return ERR_SUCCESS;
}

ERRCODE ConfManager::SerializeNodeInfo(const util::machine_node_info &info)
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
            return ERR_ERROR;
        }

        node_dat_path /= boost::filesystem::path("node.dat");

        //open file w+
        fp = fopen(node_dat_path.generic_string().c_str(), "w+");
        if (nullptr == fp)
        {
            LOG_ERROR << "ConfManager open node.dat error: fp is nullptr";
            return ERR_ERROR;
        }
    }
    catch (const std::exception & e)
    {
        LOG_ERROR << "create node error: " << e.what();
        return ERR_ERROR;
    }
    catch (const boost::exception & e)
    {
        LOG_ERROR << "create node error" << diagnostic_information(e);
        return ERR_ERROR;
    }

    assert(nullptr != fp);

    fprintf(fp, "node_id=");
    fprintf(fp, "%s",info.node_id.c_str());
    fprintf(fp, "\n");

    fprintf(fp, "node_private_key=");
    fprintf(fp, "%s",info.node_private_key.c_str());
    fprintf(fp, "\n");

    fclose(fp);

    return ERR_SUCCESS;
}

bool ConfManager::CheckNodeId()
{
    std::string node_derived = util::derive_pubkey_by_privkey(m_node_private_key);
    return node_derived == m_node_id;
}

void ConfManager::CreateCryptoKeypair() {
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

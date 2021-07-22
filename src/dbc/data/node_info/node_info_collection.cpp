#include "node_info_collection.h"
#include "node_info_query_sh.h"
#include "log/log.h"
#include "service/message/service_topic.h"
#include "server/server.h"
#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>

namespace bpo = boost::program_options;

enum {
    LINE_SIZE = 2048,
    MAX_KEY_LEN = 1280
};

static const std::string static_attrs[] = {
    "ip",
    "os",
    "cpu",
    "cpu_usage", //定期更新
    "disk",
    "mem",
    "mem_usage", //定期更新
    "version",
    "state",     //定期更新
    "wallet"
};

node_info_collection::node_info_collection() {
    for(auto& k: static_attrs) {
        m_kvs[k]="N/A";
    }
}

int32_t node_info_collection::init(const std::string& filename) {
    m_dbc_bash_filename = filename;

    if (!init_dbc_bash_sh(filename, bash_interface_str)) {
        LOG_ERROR << "fail to init shell interface for computing node!";
        return E_FILE_FAILURE;
    }

    // version
    std::string ver = STR_VER(CORE_VERSION);
    auto s = util::remove_leading_zero(ver.substr(2, 2)) + "."
             + util::remove_leading_zero(ver.substr(4, 2)) + "."
             + util::remove_leading_zero(ver.substr(6, 2)) + "."
             + util::remove_leading_zero(ver.substr(8, 2));
    m_kvs["version"] = s;

    // wallet
    std::string s_wallet;
    s_wallet = "[";
    std::vector<std::string> vec_wallets = CONF_MANAGER->get_wallets();
    for (size_t i = 0; i < vec_wallets.size(); i++) {
        if (vec_wallets[i].empty()) continue;

        if(i > 0)
            s_wallet += ",";

        s_wallet += "\"" + vec_wallets[i] + "\"";
    }
    s_wallet += "]";
    m_kvs["wallet"] = s_wallet;

    // ip os cpu disk mem
    std::string node_info_file_path = env_manager::get_home_path().generic_string() + "/.dbc_node_info.conf";
    read_node_static_info(node_info_file_path);
    return E_SUCCESS;
}

bool node_info_collection::init_dbc_bash_sh(const std::string& filename, const std::string& content) {
    std::remove(filename.c_str());
    std::ofstream foss (filename);
    if (!foss.is_open())
        return false;
    foss << content;
    foss.close();
    return true;
}

int32_t node_info_collection::read_node_static_info(const std::string& filename) {
    if (false == boost::filesystem::exists(filename) || false == boost::filesystem::is_regular_file(filename)) {
        generate_node_static_info(env_manager::get_home_path().generic_string());
    }

    bpo::options_description opts("node info options");
    for(auto& k: static_attrs) {
        opts.add_options()(k.c_str(),bpo::value<std::string>(), "");
    }

    bpo::variables_map vm;
    std::ifstream conf_ifs(filename);
    bpo::store(bpo::parse_config_file(conf_ifs, opts, true), vm);
    bpo::notify(vm);

    for(auto& k: static_attrs){
        if(vm.count(k)) {
            m_kvs[k] = vm[k].as<std::string>();
        }
    }

    return E_SUCCESS;
}

void node_info_collection::generate_node_static_info(const std::string& path) {
    std::string cmd = "/bin/bash "+ path + "/tool/node_info/node_info.sh" ;
    FILE *proc = popen(cmd.c_str(), "r");
    if (proc != nullptr) {
        char line[LINE_SIZE];
        std::string result;
        while (fgets(line, LINE_SIZE, proc))
            result += line;
        pclose(proc);
    }
}

std::vector<std::string> node_info_collection::get_all_attributes() {
    std::vector<std::string> vec;
    for(auto& k: static_attrs) {
        vec.push_back(k);
    }

    return vec;
}

void node_info_collection::refresh() {
    m_kvs["cpu_usage"] = shell_get("cpu_usage");
    m_kvs["mem_usage"] = shell_get("mem_usage");

    auto req = std::make_shared<get_task_queue_size_req_msg>();
    auto msg = std::dynamic_pointer_cast<dbc::network::message>(req);
    TOPIC_MANAGER->publish<int32_t>(typeid(get_task_queue_size_req_msg).name(), msg);
}

std::string node_info_collection::get(const std::string& key) {
    auto it = m_kvs.find(key);
    if (it == m_kvs.end()) {
        return "N/A";
    }

    std::string value = m_kvs[key];

    // task_size
    if (key == "state") {
        if (value == "0" || value == "N/A") {
            return "idle";
        }
        else if (value == "-1" ) {
            return "idle(*)";
        }
        else {
            return "busy(" + value + ")";
        }
    }

    return value;
}

void node_info_collection::set(const std::string& key, const std::string& value) {
    m_kvs[key] = value;
}

std::string node_info_collection::shell_get(const std::string& key) {
    std::string cmd = "/bin/bash " + m_dbc_bash_filename + " " + key;
    FILE *proc = popen(cmd.c_str(), "r");
    if (proc != nullptr) {
        char line[LINE_SIZE];
        std::string result;

        while (fgets(line, LINE_SIZE, proc))
            result += line;
        pclose(proc);

        util::trim(result);
        return result;
    }

    return "N/A";
}

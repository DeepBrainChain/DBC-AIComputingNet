#include "cmd_request_service.h"
#include <boost/exception/all.hpp>
#include <boost/xpressive/xpressive_dynamic.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/format.hpp>
#include "server/server.h"
#include "config/conf_manager.h"
#include "service_module/service_message_id.h"
#include "../message/protocol_coder/matrix_coder.h"
#include "../message/message_id.h"
#include "log/log.h"
#include "rapidjson/rapidjson.h"
#include "rapidjson/error/en.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"
#include "rapidjson/document.h"
#include "rapidjson/error/error.h"
#include "util/base64.h"

int32_t cmd_request_service::validate_cmd_training_task_conf(const bpo::variables_map &vm, std::string &error) {
    std::string params[] = {
            "training_engine",
            "entry_file",
            "code_dir"
    };

    for (const auto &param : params) {
        if (0 == vm.count(param)) {
            error = "missing param:" + param;
            return E_DEFAULT;
        }
    }

    std::string engine_name = vm["training_engine"].as<std::string>();
    if (engine_name.empty() || engine_name.size() > MAX_ENGINE_IMGE_NAME_LEN) {
        error = "training_engine name length exceed maximum ";
        return E_DEFAULT;
    }

    std::string entry_file_name = vm["entry_file"].as<std::string>();
    if (entry_file_name.empty() || entry_file_name.size() > MAX_ENTRY_FILE_NAME_LEN) {
        error = "entry_file name length exceed maximum ";
        return E_DEFAULT;
    }

    std::string code_dir = vm["code_dir"].as<std::string>();
    if (code_dir.empty() || E_SUCCESS != validate_ipfs_path(code_dir)) {
        error = "code_dir path is not valid";
        return E_DEFAULT;
    }

    if (0 != vm.count("data_dir") && !vm["data_dir"].as<std::string>().empty()) {
        std::string data_dir = vm["data_dir"].as<std::string>();
        if (E_SUCCESS != validate_ipfs_path(data_dir)) {
            error = "data_dir path is not valid";
            return E_DEFAULT;
        }
    }

    if (0 == vm.count("peer_nodes_list") || vm["peer_nodes_list"].as<std::vector<std::string>>().empty()) {
        if (code_dir != TASK_RESTART) {
            error = "missing param:peer_nodes_list";
            return E_DEFAULT;
        }
    } else {
        std::vector<std::string> nodes;
        nodes = vm["peer_nodes_list"].as<std::vector<std::string>>();
        for (auto &node_id : nodes) {
            if (node_id.empty())
                continue;
        }
    }

    return E_SUCCESS;
}

int32_t cmd_request_service::validate_ipfs_path(const std::string &path_arg) {
    if (path_arg == std::string(NODE_REBOOT) || path_arg == std::string(TASK_RESTART)) {
        return E_SUCCESS;
    }

    cregex reg = cregex::compile("(^[a-zA-Z0-9]{46}$)");
    if (regex_match(path_arg.c_str(), reg)) {
        return E_SUCCESS;
    }
    return E_DEFAULT;
}

int32_t cmd_request_service::validate_entry_file_name(const std::string &entry_file_name) {
    /*cregex reg = cregex::compile("(^[0-9a-zA-Z]{1,}[-_]{0,}[0-9a-zA-Z]{0,}.py$)");
    if (regex_match(entry_file_name.c_str(), reg))
    {
        return E_SUCCESS;
    }*/
    //return E_DEFAULT;
    if (entry_file_name.size() > MAX_ENTRY_FILE_NAME_LEN) {
        return E_DEFAULT;
    }

    return E_SUCCESS;
}


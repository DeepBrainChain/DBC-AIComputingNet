#ifndef DBC_CMD_REQUEST_SERVICE_H
#define DBC_CMD_REQUEST_SERVICE_H

#include "util/utils.h"
#include "service_module/service_module.h"
#include "../message/cmd_message.h"
#include "util/LruCache.hpp"
#include "data/service_info/service_info_collection.h"
#include "service/message/matrix_types.h"

class cmd_request_service : public service_module, public Singleton<cmd_request_service> {
public:
    cmd_request_service() = default;

    ~cmd_request_service() override = default;

protected:

    int32_t validate_cmd_training_task_conf(const bpo::variables_map &vm, std::string &error);

    int32_t validate_ipfs_path(const std::string &path_arg);

    int32_t validate_entry_file_name(const std::string &entry_file_name);
};

#endif

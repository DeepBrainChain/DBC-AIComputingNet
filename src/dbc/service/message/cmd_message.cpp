#include "cmd_message.h"

std::set<char> char_filter = {
        /*0x00,0x01,0x02,*/0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
                           0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c,
                           0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29,
                           0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36,
                           0x37, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f, 0x40, 0x41, 0x42, 0x43,
                           0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50,
                           0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d,
                           0x5e, 0x5f, 0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a,
                           0x6b, 0x6c, 0x6d, 0x6e, 0x6f, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
                           0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f};


//static            std::string task_start_str = "task start: ";
static std::string task_end_str = "task completed: ";
static std::string task_sh_end_str = "end to exec dbc_task.sh";

cmd_list_node_rsp::cmd_list_node_rsp() :
        op(OP_SHOW_UNKNOWN), err(""), sort("gpu") {

}

void cmd_list_node_rsp::error(std::string err_) {
    err = err_;
}

std::string cmd_list_node_rsp::to_string(std::vector<std::string> in) {
    std::string out = "";
    for (auto &item: in) {
        if (out.length()) {
            out += " , ";
        }
        out += item;
    }

    return out;
}

/**
 * return the time in second to human readable format
 * @param t time in second
 * @return  <day>:<hour>.<minute>
 */
std::string cmd_list_node_rsp::to_time_str(time_t t) {
    if (t == 0) {
        return "N/A";
    }


    time_t now = time(nullptr);
    auto d_day = 0;
    auto d_hour = 0;
    auto d_minute = 0;

    if (now <= t) {
        return "N/A";
    }


    time_t d = now - t;
    if (d < 60) {
        d_minute = 1;
    } else {
        d_day = d / (3600 * 24);
        d = d % (3600 * 24);

        d_hour = d / 3600;
        d = d % 3600;

        d_minute = d / 60;
    }

    char buf[56] = {0};
    std::snprintf(buf, sizeof(buf), "%d:%02d.%02d", d_day, d_hour, d_minute);

    const char *p = buf;
    return std::string(p);
}

void cmd_list_node_rsp::format_service_list() {
    dbc::console_printer printer;

    printer(dbc::LEFT_ALIGN, 6)(dbc::LEFT_ALIGN, 48)(dbc::LEFT_ALIGN, 17)(dbc::LEFT_ALIGN, 12)(dbc::LEFT_ALIGN, 32)(dbc::LEFT_ALIGN, 12)(
            dbc::LEFT_ALIGN, 12)(
            dbc::LEFT_ALIGN, 18);
    printer << dbc::init << "NO" << "ID" << "NAME" << "VERSION" << "GPU" << "GPU_USAGE" << "STATE"
            << "TIME" << dbc::endl;


    // order by indicated filed
    std::map<std::string, node_service_info> s_in_order;
    for (auto &it : *id_2_services) {
        std::string k;
        if (sort == "name") {
            k = it.second.name;
        } else if (sort == "timestamp") {
            k = it.second.time_stamp;
        } else {
            k = it.second.kvs[sort];
        }

        auto v = it.second;
        v.kvs["id"] = it.first;
        s_in_order[k + it.first] = v;  // "k + id" is unique
    }


    int count = 0;
    for (auto &it : s_in_order) {
        std::string ver = it.second.kvs.count("version") ? it.second.kvs["version"] : "N/A";

        std::string gpu = string_util::rtrim(it.second.kvs["gpu"], '\n');

        if (gpu.length() > 31) {
            gpu = gpu.substr(0, 31);
        }

        std::string gpu_usage = it.second.kvs.count("gpu_usage") ? it.second.kvs["gpu_usage"] : "N/A";


        std::string online_time = "N/A";
        if (it.second.kvs.count("startup_time")) {
            std::string s_time = it.second.kvs["startup_time"];
            try {
                time_t t = std::stoi(s_time);
                online_time = to_time_str(t);
            }
            catch (...) {
                //
            }
        }

        printer << dbc::init << ++count << it.second.kvs["id"] << it.second.name << ver << gpu
                << gpu_usage << it.second.kvs["state"]
                //                        << to_string(it.second.service_list)
                << online_time << dbc::endl;
    }
    printer << dbc::endl;

}

void cmd_list_node_rsp::format_node_info() {

    auto it = kvs.begin();
    //cout << "node id: " << o_node_id << endl;

    auto count = kvs.size();
    for (; it != kvs.end(); it++) {
        //cout << "******************************************************\n";
        cout << "------------------------------------------------------\n";
        if (count) {
            cout << it->first << ":\n";
        }

        if (it->first == std::string("startup_time")) {
            std::string s_time = it->second;
            try {
                time_t t = std::stoi(s_time);
                cout << std::ctime(&t) << endl;

            }
            catch (...) {
                //
                cout << "N/A \n";
            }
        } else if (it->first == std::string("docker ps")) {
            bool need_char_filter = false;
#ifdef WIN32
            need_char_filter = true;
#endif
            if (need_char_filter) {
                // remove utf-8 character that not supported by windows console
                const char *p = it->second.c_str();
                uint32_t size = it->second.length();
                for (size_t i = 0; i < size; i++) {
                    if (char_filter.find(*p) != char_filter.end()) {
                        cout << *p;
                    }
                    p++;
                }
            } else {
                cout << it->second << "\n";
            }

        } else {
            cout << it->second << "\n";
        }

        cout << "------------------------------------------------------\n";
        //cout << "******************************************************\n";
    }
    cout << "\n";

}

void cmd_list_node_rsp::format_output() {
    if (err != "") {
        cout << err << endl;
        return;
    }

    if (op == OP_SHOW_SERVICE_LIST) {
        format_service_list();
        return;
    } else if (op == OP_SHOW_NODE_INFO) {
        format_node_info();
        return;
    } else {

        LOG_ERROR << "unknown op " << op;
    }
}


void cmd_list_task_rsp::format_output_detail() {
    if (E_SUCCESS != result) {
        cout << result_info << endl;
        return;
    }

    int i = 1;
    for (auto it = task_status_list.begin(); it != task_status_list.end(); ++it, ++i) {
        std::string description = it->description;
        if (description.empty()) {
            description = "N/A";
        } else {
            // todo: only display the last 8 digits of node id
            std::string delimiter = " : ";
            size_t pos = description.find(delimiter);

            if (pos != std::string::npos) {
                if (pos > 8)
                    description = description.substr(pos - 8);
            }
        }

        std::cout << "id    : " << it->task_id << std::endl;
        std::cout << "time  : " << time_util::time_2_str(it->create_time) << std::endl;
        std::cout << "status: " << dbc::to_training_task_status_string(it->status) << std::endl;
        std::cout << "descri: " << description << std::endl;
        std::cout << "pwd   : " << it->pwd << std::endl;
        std::cout << "raw   : " << it->raw << std::endl;
//                std::cout << "passwo: " << it->pwd << std::endl;

        std::cout << std::endl;
    }
}

void cmd_list_task_rsp::format_output() {
    if (E_SUCCESS != result) {
        cout << result_info << endl;
        return;
    }

    if (task_status_list.size() == 1) {
        format_output_detail();
        return;
    }

    dbc::console_printer printer;
    printer(dbc::LEFT_ALIGN, 5)(dbc::LEFT_ALIGN, 56)(dbc::LEFT_ALIGN, 24)(dbc::LEFT_ALIGN, 30)(dbc::LEFT_ALIGN, 48);

    printer << dbc::init << "num" << "task_id" << "time" << "task_status" << "description"
            << dbc::endl;
    int i = 1;
    for (auto it = task_status_list.begin(); it != task_status_list.end(); ++it, ++i) {
        std::string description = it->description;
        if (description.empty()) {
            description = "N/A";
        } else {
            // todo: only display the last 8 digits of node id
            std::string delimiter = " : ";
            size_t pos = description.find(delimiter);

            if (pos != std::string::npos) {
                if (pos > 8)
                    description = description.substr(pos - 8);
            }
        }

        printer << dbc::init << i << it->task_id << time_util::time_2_str(
                it->create_time) << dbc::to_training_task_status_string(it->status)
                << description << dbc::endl;
    }
}

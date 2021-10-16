#include "TaskManager.h"
#include "util/crypto/utilstrencodings.h"
#include <regex>
#include <boost/property_tree/json_parser.hpp>
#include <boost/format.hpp>
#include <shadow.h>
#include "rapidjson/rapidjson.h"
#include "rapidjson/error/en.h"
#include "rapidjson/writer.h"
#include "rapidjson/document.h"
#include "rapidjson/error/error.h"
#include "log/log.h"
#include "../resource/system_info.h"
#include "../../config/conf_manager.h"

std::vector<std::string> split(const std::string &str, const std::string &delim) {
    std::vector<std::string> res;
    if ("" == str) return res;
    //先将要切割的字符串从string类型转换为char*类型
    char *strs = new char[str.length() + 1]; //不要忘了
    strcpy(strs, str.c_str());

    char *d = new char[delim.length() + 1];
    strcpy(d, delim.c_str());

    char *p = strtok(strs, d);
    while (p) {
        std::string s = p; //分割得到的字符串转换为string类型
        res.push_back(s); //存入结果数组
        p = strtok(NULL, d);
    }

    return res;
}

TaskManager::TaskManager() {

}

TaskManager::~TaskManager() {

}

FResult TaskManager::Init() {
    // load db
    FResult fret = init_db();
    int32_t ret = std::get<0>(fret);
    std::string ret_msg = std::get<1>(fret);
    if (ret != E_SUCCESS) {
        return fret;
    }

    // remove invalid tasks
    fret = remove_invalid_tasks();
    ret = std::get<0>(fret);
    ret_msg = std::get<1>(fret);
    if (ret != E_SUCCESS) {
        return fret;
    }

    // restore tasks
    fret = restore_tasks();
    ret = std::get<0>(fret);
    ret_msg = std::get<1>(fret);
    if (ret != E_SUCCESS) {
        return fret;
    }

    // remove reject iptable
    remove_reject_iptable();

    // init resource
    std::vector<std::string> taskids;
    for (auto& it : m_tasks) {
        taskids.push_back(it.first);
    }
    m_task_resource_mgr.init(taskids);
    return {E_SUCCESS, ""};
}

FResult TaskManager::init_db() {
    // tasks
    bool ret = m_task_db.init_db(env_manager::instance().get_db_path(), "task.db");
    if (!ret) {
        LOG_ERROR << "init task_db failed";
        return {E_DEFAULT, "init task_db failed"};
    }
    m_task_db.load_tasks(m_tasks);

    // iptables
    ret = m_iptable_db.init_db(env_manager::instance().get_db_path(), "iptable.db");
    if (!ret) {
        LOG_ERROR << "init iptable_db failed";
        return {E_DEFAULT, "init iptable_db failed"};
    }
    m_iptable_db.load_iptables(m_iptables);

    // sessionids
    ret = m_sessionid_db.init_db(env_manager::instance().get_db_path(), "sessionid.db");
    if (!ret) {
        LOG_ERROR << "init sessionid_db failed";
        return {E_DEFAULT, "init sessionid.db failed"};
    }

    do {
        std::map<std::string, std::shared_ptr<dbc::rent_sessionid>> wallet_sessionid;
        m_sessionid_db.load(wallet_sessionid);
        for (auto &it: wallet_sessionid) {
            m_wallet_sessionid.insert({it.first, it.second->session_id});
            m_sessionid_wallet.insert({it.second->session_id, it.first});
        }
    } while (0);

    // wallet tasks
    ret = m_wallet_task_db.init_db(env_manager::instance().get_db_path(), "rent_task.db");
    if (!ret) {
        LOG_ERROR << "init wallet_task_db failed";
        return {E_DEFAULT, "init wallet_task.db failed"};
    }
    m_wallet_task_db.load_data(m_wallet_tasks);

    return {E_SUCCESS, "ok"};
}

FResult TaskManager::remove_invalid_tasks() {
    std::string url = "qemu+tcp://localhost:16509/system";
    virConnectPtr connPtr = virConnectOpen(url.c_str());
    if (nullptr == connPtr) {
        return {E_DEFAULT, "virConnectOpen error: " + url};
    }

    std::map<std::string, std::shared_ptr<dbc::TaskInfo> > tasks = m_tasks;
    for (auto& it_task : tasks) {
        virDomainPtr domainPtr = virDomainLookupByName(connPtr, it_task.first.c_str());
        if (nullptr == domainPtr) {
            LOG_WARNING << "domain not exist: " << it_task.first;
            delete_task_data(it_task.first);
        } else {
            virDomainFree(domainPtr);
        }
    }
    virConnectClose(connPtr);

    for (auto& it : m_wallet_tasks) {
        bool bwrite = false;
        std::vector<std::string> ids;
        for (auto& task_id : it.second->task_ids) {
            auto it_task = m_tasks.find(task_id);
            if (it_task == m_tasks.end()) {
                bwrite = true;
            } else {
                ids.push_back(task_id);
            }
        }

        if (bwrite) {
            it.second->task_ids = ids;
            m_wallet_task_db.write_data(it.second);
        }
    }

    return {E_SUCCESS, "ok"};
}
 
FResult TaskManager::restore_tasks() {
    httplib::SSLClient cli(conf_manager::instance().get_dbc_chain_domain(), 443);
    std::string cur_renter_wallet;
    int64_t cur_rent_end = 0;

    std::set<std::string> wallets_set;
    for (auto& it : m_wallet_sessionid) {
        wallets_set.insert(it.first);
    }
    for (auto& it : m_wallet_tasks) {
        wallets_set.insert(it.first);
    }

    for (auto& it : wallets_set) {
        std::string str_send = R"({"jsonrpc": "2.0", "id": 1, "method":"rentMachine_getRentOrder", "params": [")"
                + it + R"(",")" + conf_manager::instance().get_node_id() + R"("]})";
        std::shared_ptr<httplib::Response> resp = cli.Post("/", str_send, "application/json");
        if (resp != nullptr) {
            do {
                rapidjson::Document doc;
                doc.Parse(resp->body.c_str());
                if (!doc.IsObject()) break;
                if (!doc.HasMember("result")) break;

                const rapidjson::Value &v_result = doc["result"];
                if (!v_result.IsObject()) break;

                if (v_result.HasMember("rentEnd")) {
                    const rapidjson::Value &v_rentEnd = v_result["rentEnd"];
                    if (!v_rentEnd.IsNumber()) break;

                    int64_t rentEnd = v_rentEnd.GetInt64();
                    if (rentEnd > 0) {
                        cur_renter_wallet = it;
                        cur_rent_end = rentEnd;
                        break;
                    }
                }
            } while (0);

            if (!cur_renter_wallet.empty()) break;
        }
    }

    std::string url = "qemu+tcp://localhost:16509/system";
    virConnectPtr connPtr = virConnectOpen(url.c_str());
    if (nullptr == connPtr) {
        return { E_DEFAULT, "virConnectOpen error" };
    }

    // 将不是当前租用者的task都停掉
    //【如果存在当前租用者，就把验证人创建的虚拟机也一起关闭】
    //【如果不存在当前租用者，就先不要关闭验证人的虚拟机】
    for (auto& it : m_wallet_tasks) {
        if (it.first != cur_renter_wallet) {
            for (auto& id : it.second->task_ids) {
                if (cur_renter_wallet.empty()) {
                    if (!util::check_id(id)) continue;
                }

                close_task(connPtr, id);
            }
        }
    }

    // 如果不存在当前租用者，恢复验证人task
    if (cur_renter_wallet.empty()) {
        std::map<std::string, std::shared_ptr<dbc::TaskInfo> > check_tasks;
        for (auto& it : m_wallet_tasks) {
            for (auto& task_id : it.second->task_ids) {
                auto iter = m_tasks.find(task_id);
                if (iter != m_tasks.end() && iter->first.find("vm_check_") != std::string::npos) {
                    check_tasks.insert({iter->first, iter->second});
                }
            }
        }

        for (auto& it_task : check_tasks) {
            int32_t task_status = it_task.second->status;
            if (task_status == ETaskStatus::TS_Running) {
                start_task(connPtr, it_task.first);
            }
        }
    }

    // 如果存在当前正在租用的用户，恢复租用者task
    if (!cur_renter_wallet.empty()) {
        // 将没有归属的task（除了验证人task）都归到当前租用的用户下
        auto it = m_wallet_tasks.find(cur_renter_wallet);
        if (it == m_wallet_tasks.end()) {
            std::map<std::string, std::shared_ptr<dbc::TaskInfo> > tmp_tasks = m_tasks;
            for (auto& it : m_wallet_tasks) {
                for (auto& id : it.second->task_ids) {
                    tmp_tasks.erase(id);
                }
            }

            std::shared_ptr<dbc::rent_task> renttask = std::make_shared<dbc::rent_task>();
            renttask->rent_wallet = cur_renter_wallet;
            renttask->rent_end = cur_rent_end;
            for (auto& id : tmp_tasks) {
                if (util::check_id(id.first)) { // 排除验证人task
                    renttask->task_ids.push_back(id.first);
                }
            }
            m_wallet_tasks[cur_renter_wallet] = renttask;
        }
 
        auto iter = m_wallet_tasks.find(cur_renter_wallet);
        for (auto& id : iter->second->task_ids) {
            auto it_task = m_tasks.find(id);
            if (it_task != m_tasks.end()) {
                int32_t task_status = it_task->second->status;
                if (task_status == ETaskStatus::TS_Running) {
                    start_task(connPtr, it_task->first);
                }
            }
        }
    }

    virConnectClose(connPtr);
    return { E_SUCCESS, "ok" };
}

void TaskManager::start_task(virConnectPtr connPtr, const std::string &task_id) {
    virDomainPtr domainPtr = virDomainLookupByName(connPtr, task_id.c_str());
    if (nullptr == domainPtr) {
        return;
    }

    virDomainInfo info;
    if (virDomainGetInfo(domainPtr, &info) < 0) {
        virDomainFree(domainPtr);
        return;
    }

    if (info.state != VIR_DOMAIN_RUNNING && info.state != VIR_DOMAIN_PAUSED) {
        if (virDomainCreate(domainPtr) < 0) {
            virDomainFree(domainPtr);
            return;
        }
    }

    auto it_taskinfo = m_tasks.find(task_id);
    if (it_taskinfo != m_tasks.end()) {
        it_taskinfo->second->status = ETaskStatus::TS_Running;
        it_taskinfo->second->operation = ETaskOp::T_OP_None;
        m_task_db.write_task(it_taskinfo->second);
        restore_iptable(task_id);
    }

    virDomainFree(domainPtr);
}

void TaskManager::close_task(virConnectPtr connPtr, const std::string &task_id) {
    virDomainPtr domainPtr = virDomainLookupByName(connPtr, task_id.c_str());
    if (nullptr == domainPtr) {
        return;
    }

    virDomainInfo info;
    if (virDomainGetInfo(domainPtr, &info) < 0) {
        virDomainFree(domainPtr);
        return;
    }

    if (info.state == VIR_DOMAIN_RUNNING || info.state == VIR_DOMAIN_PAUSED) {
        if (virDomainDestroy(domainPtr) < 0) {
            virDomainFree(domainPtr);
            return;
        }
    }

    auto it_taskinfo = m_tasks.find(task_id);
    if (it_taskinfo != m_tasks.end()) {
        it_taskinfo->second->status = ETaskStatus::TS_None;
        it_taskinfo->second->operation = ETaskOp::T_OP_None;
        m_task_db.write_task(it_taskinfo->second);
        remove_iptable(task_id);
    }

    virDomainFree(domainPtr);
}

void TaskManager::delete_task_data(const std::string &task_id) {
    auto it_task = m_tasks.find(task_id);
    if (it_task != m_tasks.end()) {
        delete_disk_system_file(task_id, it_task->second->image_name);
        delete_disk_data_file(task_id);
    }
    m_tasks.erase(task_id);
    m_task_db.delete_task(task_id);

    auto it_iptable = m_iptables.find(task_id);
    if (it_iptable != m_iptables.end()) {
        std::string host_ip = it_iptable->second->host_ip;
        std::string transform_port = it_iptable->second->ssh_port;
        std::string vm_local_ip = it_iptable->second->vm_local_ip;
        shell_delete_iptable(host_ip, transform_port, vm_local_ip);
    }
    m_iptables.erase(task_id);
    m_iptable_db.delete_iptable(task_id);

    for (auto& it : m_wallet_tasks) {
        std::shared_ptr<dbc::rent_task> it_rent_task = it.second;
        std::vector<std::string> &vec = it_rent_task->task_ids;
        bool bwrite = false;
        for (auto iter = vec.begin(); iter != vec.end(); ) {
            if (*iter == task_id) {
                iter = vec.erase(iter);
                bwrite = true;
                break;
            } else {
                ++iter;
            }
        }

        if (bwrite) {
            m_wallet_task_db.write_data(it_rent_task);
            break;
        }
    }

    m_task_resource_mgr.Delete(task_id);
}

void TaskManager::close_and_delete_task(virConnectPtr connPtr, const std::string &task_id) {
    virDomainPtr domainPtr = virDomainLookupByName(connPtr, task_id.c_str());
    if (nullptr == domainPtr) {
        return;
    }

    virDomainInfo info;
    if (virDomainGetInfo(domainPtr, &info) < 0) {
        virDomainFree(domainPtr);
        return;
    }

    if (info.state == VIR_DOMAIN_RUNNING || info.state == VIR_DOMAIN_PAUSED) {
        virDomainDestroy(domainPtr);
    }

    virDomainUndefine(domainPtr);
    virDomainFree(domainPtr);

    delete_task_data(task_id);
}

void TaskManager::delete_disk_system_file(const std::string &task_id, const std::string& disk_system_file_name) {
    std::string image = disk_system_file_name;
    auto pos = image.find('.');
    std::string real_image_name = image;
    std::string ext;
    if (pos != std::string::npos) {
        real_image_name = image.substr(0, pos);
        ext = image.substr(pos + 1);
    }
    std::string real_image_path = "/data/" + real_image_name + "_" + task_id + "." + ext;
    if (fs::is_regular_file(real_image_path)) {
        LOG_INFO << "remove disk_system file: " << real_image_path;
        remove(real_image_path.c_str());
    }
}

void TaskManager::delete_disk_data_file(const std::string &task_id) {
    DIR *dir = opendir("/data");
    std::list<std::string> files;
    struct dirent *pDir = nullptr;
    if (nullptr != dir) {
        while ((pDir = readdir(dir)) != nullptr) {
            if (!strcmp(pDir->d_name, ".") || !strcmp(pDir->d_name, "..")) {
                continue;
            }

            if (pDir->d_type == DT_DIR) {
                continue;
            } else {
                struct stat file_stat{};
                std::string filename = std::string("/data/") + pDir->d_name;
                if (stat(filename.c_str(), &file_stat) >= 0) {
                    if (S_ISDIR(file_stat.st_mode)) {
                        continue;
                    }
                }
            }

            std::string full_path_name = std::string("/data/") + pDir->d_name;
            if (full_path_name.find(task_id) != std::string::npos
                && full_path_name.find("data_") != std::string::npos) {
                files.push_back(full_path_name);
            }
        }

        closedir(dir);
    }

    for (auto& it : files) {
        if (fs::is_regular_file(it)) {
            LOG_INFO << "remove disk_data file: " << it;
            remove(it.c_str());
        }
    }
}

void TaskManager::shell_delete_iptable(const std::string &public_ip, const std::string &transform_port,
                                              const std::string &vm_local_ip) {
    std::string cmd;

    // 1804
    if (SystemInfo::instance().get_ostype() == OS_TYPE::OS_1804) {
        cmd += "sudo iptables --table nat -D PREROUTING --protocol tcp --destination " + public_ip +
                " --destination-port " + transform_port + " --jump DNAT --to-destination " + vm_local_ip + ":22";
        cmd += " && sudo iptables -t nat -D PREROUTING -p tcp --dport " + transform_port +
                " -j DNAT --to-destination " + vm_local_ip + ":22";
        auto pos = vm_local_ip.rfind('.');
        std::string ip = vm_local_ip.substr(0, pos) + ".1";
        cmd += " && sudo iptables -t nat -D POSTROUTING -p tcp --dport " + transform_port + " -d " + vm_local_ip +
                " -j SNAT --to " + ip;
        cmd += " && sudo iptables -t nat -D PREROUTING -p tcp -m tcp --dport 20000:60000 -j DNAT --to-destination " +
                vm_local_ip + ":20000-60000";
        cmd += " && sudo iptables -t nat -D PREROUTING -p udp -m udp --dport 20000:60000 -j DNAT --to-destination " +
                vm_local_ip + ":20000-60000";
        cmd += " && sudo iptables -t nat -D POSTROUTING -d " + vm_local_ip +
                " -p tcp -m tcp --dport 20000:60000 -j SNAT --to-source " + public_ip;
        cmd += " && sudo iptables -t nat -D POSTROUTING -d " + vm_local_ip +
                " -p udp -m udp --dport 20000:60000 -j SNAT --to-source " + public_ip;
    }

    // 2004
    else if (SystemInfo::instance().get_ostype() == OS_TYPE::OS_2004) {
        cmd += "sudo iptables -F -t nat";
        cmd += " && sudo iptables -t nat -D PREROUTING -p tcp -d " + public_ip + " --dport " + transform_port
                + " -j DNAT --to-destination " + vm_local_ip + ":22";
        auto pos = vm_local_ip.rfind('.');
        std::string ip = vm_local_ip.substr(0, pos) + ".0/24";
        cmd += " && sudo iptables -t nat -D POSTROUTING -s " + ip + " -j SNAT --to-source " + public_ip;
        cmd += " && sudo iptables -t nat -D PREROUTING -p tcp -d " + public_ip + " --dport 6000:60000 -j DNAT --to-destination "
                + vm_local_ip + ":6000-60000";
    }

    run_shell(cmd.c_str());
}

void TaskManager::shell_add_iptable(const std::string &public_ip, const std::string &transform_port,
                                       const std::string &vm_local_ip) {
    // 1804
    if (SystemInfo::instance().get_ostype() == OS_TYPE::OS_1804) {
        std::string rule = "sudo iptables -t nat -C PREROUTING --protocol tcp --destination " + public_ip +
                " --destination-port " + transform_port + " --jump DNAT --to-destination " + vm_local_ip + ":22";
        std::string ret = run_shell(rule.c_str());
        if (ret.find("No") != std::string::npos) {
            util::replace(rule, "-C", "-I");
            run_shell(rule.c_str());
        }

        rule = "sudo iptables -t nat -C PREROUTING -p tcp --dport " + transform_port +
                " -j DNAT --to-destination " + vm_local_ip + ":22";
        ret = run_shell(rule.c_str());
        if (ret.find("No") != std::string::npos) {
            util::replace(rule, "-C", "-I");
            run_shell(rule.c_str());
        }

        auto pos = vm_local_ip.rfind('.');
        std::string ip = vm_local_ip.substr(0, pos) + ".1";
        rule = "sudo iptables -t nat -C POSTROUTING -p tcp --dport " + transform_port + " -d " + vm_local_ip +
                " -j SNAT --to " + ip;
        ret = run_shell(rule.c_str());
        if (ret.find("No") != std::string::npos) {
            util::replace(rule, "-C", "-I");
            run_shell(rule.c_str());
        }

        rule = "sudo iptables -t nat -C PREROUTING -p tcp -m tcp --dport 20000:60000 -j DNAT --to-destination " +
                vm_local_ip + ":20000-60000";
        ret = run_shell(rule.c_str());
        if (ret.find("No") != std::string::npos) {
            util::replace(rule, "-C", "-I");
            run_shell(rule.c_str());
        }

        rule = "sudo iptables -t nat -C PREROUTING -p udp -m udp --dport 20000:60000 -j DNAT --to-destination " +
                vm_local_ip + ":20000-60000";
        ret = run_shell(rule.c_str());
        if (ret.find("No") != std::string::npos) {
            util::replace(rule, "-C", "-I");
            run_shell(rule.c_str());
        }

        rule = "sudo iptables -t nat -C POSTROUTING -d " + vm_local_ip +
                " -p tcp -m tcp --dport 20000:60000 -j SNAT --to-source " + public_ip;
        ret = run_shell(rule.c_str());
        if (ret.find("No") != std::string::npos) {
            util::replace(rule, "-C", "-I");
            run_shell(rule.c_str());
        }

        rule = "sudo iptables -t nat -C POSTROUTING -d " + vm_local_ip +
                " -p udp -m udp --dport 20000:60000 -j SNAT --to-source " + public_ip;
        ret = run_shell(rule.c_str());
        if (ret.find("No") != std::string::npos) {
            util::replace(rule, "-C", "-I");
            run_shell(rule.c_str());
        }
    }

    // 2004
    else if (SystemInfo::instance().get_ostype() == OS_TYPE::OS_2004) {
        std::string rule = "sudo iptables -t nat -C PREROUTING -p tcp -d " + public_ip + " --dport " + transform_port
                + " -j DNAT --to-destination " + vm_local_ip + ":22";
        std::string ret = run_shell(rule.c_str());
        if (ret.find("No") != std::string::npos) {
            util::replace(rule, "-C", "-I");
            run_shell(rule.c_str());
        }

        auto pos = vm_local_ip.rfind('.');
        std::string local_ip = vm_local_ip.substr(0, pos) + ".0/24";
        rule = "sudo iptables -t nat -C POSTROUTING -s " + local_ip + " -j SNAT --to-source " + public_ip;
        ret = run_shell(rule.c_str());
        if (ret.find("No") != std::string::npos) {
            util::replace(rule, "-C", "-I");
            run_shell(rule.c_str());
        }

        rule = "sudo iptables -t nat -C PREROUTING -p tcp -d " + public_ip + " --dport 6000:60000 -j DNAT --to-destination "
                + vm_local_ip + ":6000-60000";
        ret = run_shell(rule.c_str());
        if (ret.find("No") != std::string::npos) {
            util::replace(rule, "-C", "-I");
            run_shell(rule.c_str());
        }
    }
}

void TaskManager::remove_reject_iptable() {
    std::string cmd = "sudo iptables -t filter --list-rules |grep reject |awk '{print $0}' |tr \"\n\" \"|\"";
    std::string str = run_shell(cmd.c_str());
    std::vector<std::string> vec;
    util::split(str, "|", vec);
    for (auto & it : vec) {
        if (it.find("-A") != std::string::npos) {
            util::replace(it, "-A", "-D");
            std::string cmd = "sudo iptables -t filter " + it;
            run_shell(cmd.c_str());
        }
    }
}

void TaskManager::remove_iptable(const std::string& task_id) {
    auto it = m_iptables.find(task_id);
    if (it != m_iptables.end()) {
        std::string host_ip = it->second->host_ip;
        std::string transform_port = it->second->ssh_port;
        std::string vm_local_ip = it->second->vm_local_ip;
        shell_delete_iptable(host_ip, transform_port, vm_local_ip);
    }
}

void TaskManager::restore_iptable(const std::string &task_id) {
    auto it = m_iptables.find(task_id);
    if (it != m_iptables.end()) {
        std::string host_ip = it->second->host_ip;
        std::string transform_port = it->second->ssh_port;
        std::string vm_local_ip = it->second->vm_local_ip;
        shell_add_iptable(host_ip, transform_port, vm_local_ip);
    }
}

//////////////////////////////////////////////////////////////////////////////////////

FResult TaskManager::CreateTask(const std::string& wallet, const std::string &task_id,
                                const std::string &login_password, const std::string &additional,
                                int64_t rent_end, USER_ROLE role) {
    auto it = m_tasks.find(task_id);
    if (it != m_tasks.end()) {
        return {E_DEFAULT, "task_id already exist"};
    }

    if (login_password.empty()) {
        return {E_DEFAULT, "login_password is empty"};
    }

    if (additional.empty()) {
        return {E_DEFAULT, "additional is empty"};
    }

    rapidjson::Document doc;
    doc.Parse(additional.c_str());
    if (!doc.IsObject()) {
        return {E_DEFAULT, "additional(json) parse failed"};
    }

    std::string image_name, ssh_port, s_gpu_count, s_cpu_cores, s_mem_rate;
    JSON_PARSE_STRING(doc, "image_name", image_name)
    JSON_PARSE_STRING(doc, "ssh_port", ssh_port)
    JSON_PARSE_STRING(doc, "gpu_count", s_gpu_count)
    JSON_PARSE_STRING(doc, "cpu_cores", s_cpu_cores)
    JSON_PARSE_STRING(doc, "mem_rate", s_mem_rate)
    std::string vm_xml, vm_xml_url;
    JSON_PARSE_STRING(doc, "vm_xml", vm_xml)
    JSON_PARSE_STRING(doc, "vm_xml_url", vm_xml_url)

    // check字段格式
    /*
    if (vm_xml.empty() && vm_xml_url.empty()) {
    */
        if (image_name.empty() || image_name.substr(image_name.size() - 5) != "qcow2") {
            return {E_DEFAULT, "image_name is empty or image_name is invalid"};
        }

        std::regex reg("^[\\d]*[.]?[\\d]*$");
        if (!std::regex_match(ssh_port, reg) || atoi(ssh_port.c_str()) <= 0) {
            return {E_DEFAULT, "ssh_port is invalid"};
        }

        if (!std::regex_match(s_gpu_count, reg) || atoi(s_gpu_count.c_str()) < 0) {
            return {E_DEFAULT, "gpu_count is invalid"};
        }

        if (!std::regex_match(s_cpu_cores, reg) || atoi(s_cpu_cores.c_str()) <= 0) {
            return {E_DEFAULT, "cpu_cores is invalid"};
        }

        if (!std::regex_match(s_mem_rate, reg) || atof(s_mem_rate.c_str()) <= 0) {
            return {E_DEFAULT, "mem_rate is invalid"};
        }
    /*
    } else {
        if (vm_xml_url.empty()) {
            std::string real_xml_name = vm_xml.substr(0, vm_xml.size() - 4) + "_" + task_id + ".xml";
            real_xml_name = "/data/" + real_xml_name;
            std::string file_content;
            boost::filesystem::load_string_file(real_xml_name, file_content);
            rapidjson::Document doc;
            rapidjson::ParseResult ok = doc.Parse(file_content.c_str());
            if (!ok) {
                return {E_DEFAULT, std::string("json parse error: ") +
                                   rapidjson::GetParseError_En(ok.Code()) + "(" + std::to_string(ok.Offset()) +
                                   ")"};
            }

            //todo: 检查资源够不够

        } else {
            //todo: 下载xml文件，并检查资源够不够

        }
    }
    */

    // check资源够不够
    int32_t cpu_count = atoi(s_cpu_cores.c_str());
    //cpu数量必须是(sockets * threads)的整数倍
    int32_t sockets = SystemInfo::instance().get_cpuinfo().physical_cores;
    int32_t threads = SystemInfo::instance().get_cpuinfo().threads_per_cpu;
    int32_t left = cpu_count % (sockets * threads);
    if (left != 0)
        cpu_count += (sockets * threads) - left;
    int32_t gpu_count = atoi(s_gpu_count.c_str());
    float mem_rate = atof(s_mem_rate.c_str());

    if (cpu_count > SystemInfo::instance().get_cpuinfo().total_cores)
        cpu_count = SystemInfo::instance().get_cpuinfo().total_cores;

    if (role == USER_ROLE::UR_VERIFIER) {
        cpu_count = SystemInfo::instance().get_cpuinfo().total_cores;
    }

    if (!check_cpu(cpu_count)) {
        LOG_ERROR << "check cpu failed";
        return {E_DEFAULT, "cpu not enough"};
    }

    if (gpu_count > SystemInfo::instance().get_gpuinfo().size())
        gpu_count = SystemInfo::instance().get_gpuinfo().size();

    if (role == USER_ROLE::UR_VERIFIER) {
        gpu_count = SystemInfo::instance().get_gpuinfo().size();
    }

    if (!check_gpu(gpu_count)) {
        LOG_ERROR << "check gpu failed";
        return {E_DEFAULT, "gpu not enough"};
    }

    if (mem_rate > 1.0f)
        mem_rate = 1.0f;

    if (role == USER_ROLE::UR_VERIFIER) {
        mem_rate = 1.0f;
    }

    if (!check_mem(mem_rate)) {
        LOG_ERROR << "check mem failed";
        return {E_DEFAULT, "mem not enough"};
    }

    LOG_INFO << "cpu gpu mem check ok";

    std::shared_ptr<dbc::TaskInfo> taskinfo = std::make_shared<dbc::TaskInfo>();
    taskinfo->__set_task_id(task_id);
    taskinfo->__set_image_name(image_name);
    taskinfo->__set_login_password(login_password);
    taskinfo->__set_ssh_port(ssh_port);
    taskinfo->__set_status(TS_Creating);
    taskinfo->__set_operation(T_OP_Create);
    int64_t now = time(nullptr);
    taskinfo->__set_create_time(now);
    taskinfo->__set_last_start_time(now);
    taskinfo->hardware_resource.__set_gpu_count(gpu_count);
    taskinfo->hardware_resource.__set_cpu_cores(cpu_count);
    taskinfo->hardware_resource.__set_mem_rate(mem_rate);
    taskinfo->__set_vm_xml(vm_xml);
    taskinfo->__set_vm_xml_url(vm_xml_url);

    // add task resource
    add_task_resource(task_id, cpu_count, mem_rate, gpu_count);

    m_tasks[task_id] = taskinfo;
    m_task_db.write_task(taskinfo);

    std::shared_ptr<dbc::rent_task> rent_task = std::make_shared<dbc::rent_task>();
    rent_task->rent_wallet = wallet;
    rent_task->rent_end = rent_end;
    rent_task->task_ids.push_back(task_id);
    m_wallet_tasks[wallet] = rent_task;
    m_wallet_task_db.write_data(rent_task);

    m_process_tasks.push_back(taskinfo);

    return {E_SUCCESS, ""};
}

bool TaskManager::check_cpu(int32_t cpu_count) {
    int32_t sockets = SystemInfo::instance().get_cpuinfo().physical_cores;
    int32_t cores_per_socket = SystemInfo::instance().get_cpuinfo().physical_cores_per_cpu;
    int32_t threads_per_core = SystemInfo::instance().get_cpuinfo().threads_per_cpu;

    int32_t cur_cores = 0;
    for (auto& it : m_tasks) {
        if (it.second->status != TS_None) {
            const TaskResource& task_resource = m_task_resource_mgr.GetTaskResource(it.first);
            cur_cores += task_resource.physical_cores_per_cpu;
        }
    }

    int32_t can_use = cores_per_socket - cur_cores;
    int32_t need_count = cpu_count / (sockets * threads_per_core);
    if (need_count <= 0) need_count = 1;
    return can_use >= need_count;
}

bool TaskManager::check_gpu(int32_t gpu_count) {
    const std::map<std::string, gpu_info>& gpulist = SystemInfo::instance().get_gpuinfo();

    int32_t cur_gpus = 0;
    for (auto& it : m_tasks) {
        if (it.second->status != TS_None) {
            const TaskResource& task_resource = m_task_resource_mgr.GetTaskResource(it.first);
            cur_gpus += task_resource.gpus.size();
        }
    }

    return (gpulist.size() - cur_gpus) >= gpu_count;
}

bool TaskManager::check_mem(float mem_rate) {
    uint64_t mem_total = SystemInfo::instance().get_meminfo().mem_total; //KB

    uint64_t cur_mem = 0;
    for (auto& it : m_tasks) {
        if (it.second->status != TS_None) {
            const TaskResource& task_resource = m_task_resource_mgr.GetTaskResource(it.first);
            cur_mem += task_resource.mem_size;
        }
    }

    uint64_t need_size = mem_total * (double)mem_rate;
    return (mem_total - cur_mem) >= need_size;
}

void TaskManager::add_task_resource(const std::string& task_id, int32_t cpu_count, float mem_rate, int32_t gpu_count) {
    TaskResource task_resource;
    int32_t threads_count = SystemInfo::instance().get_cpuinfo().threads_per_cpu;
    int32_t sockets_count = SystemInfo::instance().get_cpuinfo().physical_cores;
    int32_t cores_per_socket = SystemInfo::instance().get_cpuinfo().physical_cores_per_cpu;
    task_resource.physical_cpu = sockets_count;
    task_resource.threads_per_cpu = threads_count;
    task_resource.physical_cores_per_cpu = cpu_count / (sockets_count * threads_count);
    if (task_resource.physical_cores_per_cpu <= 0) task_resource.physical_cores_per_cpu = 1;
    LOG_INFO << "socket: " << task_resource.physical_cpu
             << ", cores: " << task_resource.physical_cores_per_cpu
             << ", threads: " << task_resource.threads_per_cpu;

    int64_t mem_total = SystemInfo::instance().get_meminfo().mem_total; // KB
    task_resource.mem_size = (mem_total * (double)mem_rate > 1000000000) ? 1000000000 : (mem_total * (double)mem_rate);
    LOG_INFO << "memory: " << size_to_string(task_resource.mem_size, 1024L);

    std::map<std::string, gpu_info> can_use_gpu = SystemInfo::instance().get_gpuinfo();
    for (auto& it : m_tasks) {
        if (it.second->status != TS_None) {
            const TaskResource& resource = m_task_resource_mgr.GetTaskResource(it.first);
            const std::map<std::string, std::list<std::string>>& gpus = resource.gpus;
            for (auto& it1 : gpus) {
                can_use_gpu.erase(it1.first);
            }
        }
    }

    int cur_count = 0;
    for (auto& it : can_use_gpu) {
        task_resource.gpus[it.first] = it.second.devices;

        cur_count++;
        if (cur_count == gpu_count) break;
    }

    std::string str_gpu;
    const std::map<std::string, std::list<std::string>>& task_gpus = task_resource.gpus;
    for (auto& it : task_gpus)
        str_gpu += it.first + " | ";
    LOG_INFO << "gpus: " << str_gpu;

    task_resource.disk_system_size = g_disk_system_size * 1024L; // MB
    int64_t disk_total_size = SystemInfo::instance().get_diskinfo().disk_total / 1024; // MB
    task_resource.disks_data[1] = (disk_total_size - g_disk_system_size * 1024L) * 0.75;
    LOG_INFO << "disk_system_size:" << size_to_string(task_resource.disk_system_size, 1024L * 1024L)
             << ", disk_data_1: " << size_to_string(task_resource.disks_data[1], 1024L * 1024L);

    m_task_resource_mgr.AddTaskResource(task_id, task_resource);
}

FResult TaskManager::StartTask(const std::string& wallet, const std::string &task_id) {
    auto it = m_tasks.find(task_id);
    if (it == m_tasks.end()) {
        return {E_DEFAULT, "task_id not exist"};
    }

    auto taskinfo = it->second;
    if (taskinfo->operation != T_OP_None) {
        return {E_DEFAULT, "task is busy, please try again later"};
    }

    EVmStatus vm_status = m_vm_client.GetDomainStatus(task_id);
    if (vm_status == VS_SHUT_OFF) {
        taskinfo->__set_status(TS_Starting);
        taskinfo->__set_operation(T_OP_Start);
        taskinfo->__set_last_start_time(time(nullptr));
        m_process_tasks.push_back(taskinfo);

        m_task_db.write_task(taskinfo);
        return {E_SUCCESS, ""};
    } else {
        return {E_DEFAULT, "task is already starting"};
    }
}

FResult TaskManager::StopTask(const std::string& wallet, const std::string &task_id) {
    auto it = m_tasks.find(task_id);
    if (it == m_tasks.end()) {
        return {E_DEFAULT, "task_id not exist"};
    }

    auto taskinfo = it->second;
    if (taskinfo->operation != T_OP_None) {
        return {E_DEFAULT, "task is busy, please try again later"};
    }

    EVmStatus vm_status = m_vm_client.GetDomainStatus(task_id);
    if (vm_status == VS_RUNNING) {
        taskinfo->__set_status(TS_Stopping);
        taskinfo->__set_operation(T_OP_Stop);
        taskinfo->__set_last_stop_time(time(nullptr));
        m_process_tasks.push_back(taskinfo);

        m_task_db.write_task(taskinfo);
        return {E_SUCCESS, ""};
    } else {
        return {E_DEFAULT, "task is not running"};
    }
}

FResult TaskManager::RestartTask(const std::string& wallet, const std::string &task_id) {
    auto it = m_tasks.find(task_id);
    if (it == m_tasks.end()) {
        return {E_DEFAULT, "task_id not exist"};
    }

    auto taskinfo = it->second;
    if (taskinfo->operation != T_OP_None) {
        return {E_DEFAULT, "task is busy, please try again later"};
    }

    EVmStatus vm_status = m_vm_client.GetDomainStatus(task_id);
    if (vm_status == VS_SHUT_OFF || vm_status == VS_RUNNING) {
        taskinfo->__set_status(TS_Restarting);
        taskinfo->__set_operation(T_OP_ReStart);
        taskinfo->__set_last_start_time(time(nullptr));
        m_process_tasks.push_back(taskinfo);
        m_task_db.write_task(taskinfo);
        return {E_SUCCESS, ""};
    } else {
        return {E_DEFAULT, "task is starting..."};
    }
}

FResult TaskManager::ResetTask(const std::string& wallet, const std::string &task_id) {
    auto it = m_tasks.find(task_id);
    if (it == m_tasks.end()) {
        return {E_DEFAULT, "task_id not exist"};
    }

    auto taskinfo = it->second;
    if (taskinfo->operation != T_OP_None) {
        return {E_DEFAULT, "task is busy, please try again later"};
    }

    EVmStatus vm_status = m_vm_client.GetDomainStatus(task_id);
    if (vm_status == VS_RUNNING) {
        taskinfo->__set_status(TS_Resetting);
        taskinfo->__set_operation(T_OP_Reset);
        taskinfo->__set_last_start_time(time(nullptr));
        m_process_tasks.push_back(taskinfo);
        m_task_db.write_task(taskinfo);
        return {E_SUCCESS, ""};
    } else {
        return {E_DEFAULT, "task is not running"};
    }
}

FResult TaskManager::DeleteTask(const std::string& wallet, const std::string &task_id) {
    auto it = m_tasks.find(task_id);
    if (it == m_tasks.end()) {
        return {E_DEFAULT, "task_id not exist"};
    }

    auto taskinfo = it->second;
    if (taskinfo->operation != T_OP_None) {
        return {E_DEFAULT, "task is busy, please try again later"};
    }

    EVmStatus vm_status = m_vm_client.GetDomainStatus(task_id);
    if (vm_status == VS_SHUT_OFF || vm_status == VS_RUNNING) {
        taskinfo->__set_status(TS_Deleting);
        taskinfo->__set_operation(T_OP_Delete);
        taskinfo->__set_last_stop_time(time(nullptr));
        m_process_tasks.push_back(taskinfo);
        m_task_db.write_task(taskinfo);
        return {E_SUCCESS, ""};
    } else {
        return {E_DEFAULT, "please try again after a minute"};
    }
}

std::shared_ptr<dbc::TaskInfo> TaskManager::FindTask(const std::string& wallet, const std::string &task_id) {
    std::shared_ptr<dbc::TaskInfo> ptr = nullptr;
    auto it = m_wallet_tasks.find(wallet);
    if (it != m_wallet_tasks.end()) {
        for (auto& it_id : it->second->task_ids) {
            if (it_id == task_id) {
                auto it_task = m_tasks.find(it_id);
                if (it_task != m_tasks.end()) {
                    ptr = it_task->second;
                    break;
                }
            }
        }
    }

    return ptr;
}

FResult TaskManager::GetTaskLog(const std::string &task_id, ETaskLogDirection direction, int32_t nlines,
                                std::string &log_content) {
    auto it = m_tasks.find(task_id);
    if (it == m_tasks.end()) {
        log_content = "";
        return {E_DEFAULT, "task_id not exist"};
    } else {
        log_content = m_vm_client.GetDomainLog(task_id, direction, nlines);

        if (GET_LOG_HEAD == (ETaskLogDirection) direction) {
            log_content = log_content.substr(0, MAX_LOG_CONTENT_SIZE);
        }
        else {
            size_t log_lenth = log_content.length();
            if (log_lenth > MAX_LOG_CONTENT_SIZE) {
                log_content = log_content.substr(log_lenth - MAX_LOG_CONTENT_SIZE, MAX_LOG_CONTENT_SIZE);
            }
        }

        return {E_SUCCESS, ""};
    }
}

void TaskManager::ListAllTask(const std::string& wallet, std::vector<std::shared_ptr<dbc::TaskInfo>> &vec) {
    auto it = m_wallet_tasks.find(wallet);
    if (it != m_wallet_tasks.end()) {
        for (auto& it_id : it->second->task_ids) {
            auto it_task = m_tasks.find(it_id);
            if (it_task != m_tasks.end()) {
                vec.push_back(it_task->second);
            }
        }
    }
}

void TaskManager::ListRunningTask(const std::string& wallet, std::vector<std::shared_ptr<dbc::TaskInfo>> &vec) {
    auto it = m_wallet_tasks.find(wallet);
    if (it != m_wallet_tasks.end()) {
        for (auto& it_id : it->second->task_ids) {
            auto it_task = m_tasks.find(it_id);
            if (it_task != m_tasks.end()) {
                if (it_task->second->status == TS_Running)
                    vec.push_back(it_task->second);
            }
        }
    }
}

int32_t TaskManager::GetRunningTaskSize(const std::string& wallet) {
    int32_t count = 0;
    auto it = m_wallet_tasks.find(wallet);
    if (it != m_wallet_tasks.end()) {
        for (auto& it_id : it->second->task_ids) {
            auto it_task = m_tasks.find(it_id);
            if (it_task != m_tasks.end()) {
                if (it_task->second->status != TS_None)
                    count += 1;
            }
        }
    }

    return count;
}

ETaskStatus TaskManager::GetTaskStatus(const std::string &task_id) {
    auto it = m_tasks.find(task_id);
    if (it == m_tasks.end()) {
        return TS_None;
    } else {
        return (ETaskStatus) it->second->status;
    }
}

void TaskManager::DeleteOtherCheckTask(const std::string& wallet) {
    std::vector<std::string> check_ids;
    for (auto& it : m_wallet_tasks) {
        if (it.first != wallet) {
            for (auto& it_id : it.second->task_ids) {
                auto it_task = m_tasks.find(it_id);
                if (it_task != m_tasks.end()) {
                    if (it_task->second->task_id.find("vm_check_") != std::string::npos) {
                        check_ids.push_back(it_task->second->task_id);
                    }
                }
            }
        }
    }

    std::string url = "qemu+tcp://localhost:16509/system";
    virConnectPtr connPtr = virConnectOpen(url.c_str());
    if (nullptr == connPtr) {
        return;
    }

    for (auto& task_id : check_ids) {
        close_and_delete_task(connPtr, task_id);
    }

    virConnectClose(connPtr);
}

void TaskManager::DeleteAllCheckTasks() {
    std::vector<std::string> check_ids;
    for (auto& it : m_tasks) {
        if (it.second->task_id.find("vm_check_") != std::string::npos) {
            check_ids.push_back(it.second->task_id);
        }
    }

    std::string url = "qemu+tcp://localhost:16509/system";
    virConnectPtr connPtr = virConnectOpen(url.c_str());
    if (nullptr == connPtr) {
        return;
    }

    for (auto& task_id : check_ids) {
        close_and_delete_task(connPtr, task_id);
    }

    virConnectClose(connPtr);
}

std::string TaskManager::CheckSessionId(const std::string &session_id, const std::string &session_id_sign) {
    auto it = m_sessionid_wallet.find(session_id);
    if (it == m_sessionid_wallet.end()) {
        return "";
    }

    std::string rent_wallet = it->second;
    if (util::verify_sign(session_id_sign, session_id, rent_wallet)) {
        return it->second;
    } else {
        return "";
    }
}

std::string TaskManager::CreateSessionId(const std::string &wallet) {
    auto it = m_wallet_sessionid.find(wallet);
    if (it == m_wallet_sessionid.end()) {
        std::string id = util::create_session_id();
        m_wallet_sessionid[wallet] = id;
        m_wallet_sessionid[id] = wallet;
        std::shared_ptr<dbc::rent_sessionid> sessionid(new dbc::rent_sessionid());
        sessionid->rent_wallet = wallet;
        sessionid->session_id = id;
        m_sessionid_db.write(sessionid);
        return id;
    } else {
        return it->second;
    }
}

std::string TaskManager::GetSessionId(const std::string &wallet) {
    auto it = m_wallet_sessionid.find(wallet);
    if (it != m_wallet_sessionid.end()) {
        return it->second;
    } else {
        return "";
    }
}

bool TaskManager::set_vm_password(const std::string &domain_name, const std::string &username, const std::string &pwd) {
    bool ret = false;
    virConnectPtr conn_ptr = nullptr;
    virDomainPtr domain_ptr = nullptr;
    do {
        conn_ptr = virConnectOpen("qemu+tcp://localhost/system");
        if (conn_ptr == nullptr) {
            ret = false;
            virErrorPtr error = virGetLastError();
            LOG_ERROR << "connect virt error: " << error->message;
            virFreeError(error);
            break;
        }

        domain_ptr = virDomainLookupByName(conn_ptr, domain_name.c_str());
        if (domain_ptr == nullptr) {
            ret = false;
            virErrorPtr error = virGetLastError();
            LOG_ERROR << "virDomainLookupByName error: " << error->message;
            virFreeError(error);
            break;
        }

        int try_count = 0;
        int succ = -1;
        // max: 5min
        while (succ != 0 && try_count < 100) {
            LOG_INFO << "set_vm_password try_count: " << (try_count + 1);
            succ = virDomainSetUserPassword(domain_ptr, username.c_str(), pwd.c_str(), 0);
            if (succ != 0) {
                virErrorPtr error = virGetLastError();
                LOG_ERROR << "virDomainSetUserPassword error: " << error->message;
                virFreeError(error);
            }

            try_count++;
            sleep(3);
        }

        if (succ == 0) {
            ret = true;
            LOG_INFO << "set vm password successful, task_id:" << domain_name << ", user:" << username << ", pwd:"
                     << pwd;
        } else {
            ret = false;
            LOG_INFO << "set vm password failed, task_id:" << domain_name << ", user:" << username << ", pwd:"
                     << pwd;
        }
    } while(0);

    if (domain_ptr != nullptr) {
        virDomainFree(domain_ptr);
    }

    if (conn_ptr != nullptr) {
        virConnectClose(conn_ptr);
    }

    return ret;
}

std::string TaskManager::get_vm_local_ip(const std::string &domain_name) {
    std::string vm_local_ip;
    virConnectPtr connPtr = nullptr;
    virDomainPtr domainPtr = nullptr;

    do {
        connPtr = virConnectOpen("qemu+tcp://localhost:16509/system");
        if (nullptr == connPtr) {
            break;
        }

        domainPtr = virDomainLookupByName(connPtr, domain_name.c_str());
        if (nullptr == domainPtr) {
            break;
        }

        int32_t try_count = 0;
        while (vm_local_ip.empty() && try_count < 1000) {
            LOG_INFO << "transform_port try_count: " << (try_count + 1);
            virDomainInterfacePtr *ifaces = nullptr;
            int ifaces_count = virDomainInterfaceAddresses(domainPtr, &ifaces,
                                                           VIR_DOMAIN_INTERFACE_ADDRESSES_SRC_LEASE, 0);
            for (int i = 0; i < ifaces_count; i++) {
                /*
                std::string if_name = ifaces[i]->name;
                std::string if_hwaddr;
                if (ifaces[i]->hwaddr)
                    if_hwaddr = ifaces[i]->hwaddr;
                */
                for (int j = 0; j < ifaces[i]->naddrs; j++) {
                    virDomainIPAddressPtr ip_addr = ifaces[i]->addrs + j;
                    vm_local_ip = ip_addr->addr;
                    if (!vm_local_ip.empty()) break;
                    //printf("[addr: %s prefix: %d type: %d]", ip_addr->addr, ip_addr->prefix, ip_addr->type);
                }

                if (!vm_local_ip.empty()) break;
            }

            try_count += 1;
            sleep(3);
        }
    } while(0);

    if (nullptr != domainPtr) {
        virDomainFree(domainPtr);
    }

    if (nullptr != connPtr) {
        virConnectClose(connPtr);
    }

    return vm_local_ip;
}

bool TaskManager::create_task_iptable(const std::string &domain_name, const std::string &transform_port) {
    std::string public_ip = SystemInfo::instance().get_publicip();
    if (!public_ip.empty() && !transform_port.empty()) {
        std::string vm_local_ip = get_vm_local_ip(domain_name);
        if (!vm_local_ip.empty()) {
            shell_add_iptable(public_ip, transform_port, vm_local_ip);

            std::shared_ptr<dbc::task_iptable> iptable(new dbc::task_iptable());
            iptable->task_id = domain_name;
            iptable->host_ip = public_ip;
            iptable->vm_local_ip = vm_local_ip;
            iptable->ssh_port = transform_port;
            m_iptables[domain_name] = iptable;
            m_iptable_db.write_iptable(iptable);

            LOG_INFO << "transform_port successful, public_ip:" << public_ip << " ssh_port:" << transform_port
                     << " local_ip:" << vm_local_ip;
            return true;
        } else {
            LOG_ERROR << "transform_port failed: vm_local_ip is empty";
            return false;
        }
    } else {
        LOG_ERROR << "transform_port failed, ip or port is empty" << public_ip << ":" << transform_port;
        return false;
    }
}

void TaskManager::ProcessTask() {
    LOG_INFO << "TaskManager::ProcessTask (" << m_process_tasks.size() << ")";

    if (m_process_tasks.empty()) return;

    auto taskinfo = m_process_tasks.front();
    m_process_tasks.pop_front();

    if (taskinfo->operation == T_OP_Create) {
        const TaskResource& task_resource = m_task_resource_mgr.GetTaskResource(taskinfo->task_id);

        if (E_SUCCESS == m_vm_client.CreateDomain(taskinfo->task_id, taskinfo->image_name, task_resource)) {
            bool succ = create_task_iptable(taskinfo->task_id, taskinfo->ssh_port);
            if (!succ) {
                m_vm_client.DestoryDomain(taskinfo->task_id);
                m_vm_client.UndefineDomain(taskinfo->task_id);
                delete_task_data(taskinfo->task_id);

                LOG_ERROR << "transform failed (" << taskinfo->task_id << ")";
            } else {
                succ = set_vm_password(taskinfo->task_id, "dbc", taskinfo->login_password);
                if (!succ) {
                    m_vm_client.DestoryDomain(taskinfo->task_id);
                    m_vm_client.UndefineDomain(taskinfo->task_id);
                    delete_task_data(taskinfo->task_id);

                    LOG_ERROR << "set_vm_password failed (" << taskinfo->task_id << ")";
                } else {
                    taskinfo->__set_status(TS_Running);
                    taskinfo->__set_operation(T_OP_None);
                    m_task_db.write_task(taskinfo);

                    LOG_INFO << "create task " << taskinfo->task_id << " successful";
                }
            }
        } else {
            delete_task_data(taskinfo->task_id);

            LOG_ERROR << "create task " << taskinfo->task_id << " failed";
        }
    } else if (taskinfo->operation == T_OP_Start) {
        if (E_SUCCESS == m_vm_client.StartDomain(taskinfo->task_id)) {
            taskinfo->__set_status(TS_Running);
            taskinfo->__set_operation(T_OP_None);
            m_task_db.write_task(taskinfo);

            LOG_INFO << "start task " << taskinfo->task_id << " successful";
        } else {
            taskinfo->__set_operation(T_OP_None);
            m_task_db.write_task(taskinfo);

            LOG_ERROR << "start task " << taskinfo->task_id << " failed";
        }
    } else if (taskinfo->operation == T_OP_Stop) {
        if (E_SUCCESS == m_vm_client.DestoryDomain(taskinfo->task_id)) {
            taskinfo->__set_status(TS_None);
            taskinfo->__set_operation(T_OP_None);
            m_task_db.write_task(taskinfo);

            LOG_INFO << "stop task " << taskinfo->task_id << " successful";
        } else {
            taskinfo->__set_operation(T_OP_None);
            m_task_db.write_task(taskinfo);

            LOG_ERROR << "stop task " << taskinfo->task_id << " failed";
        }
    } else if (taskinfo->operation == T_OP_ReStart) {
        EVmStatus vm_status = m_vm_client.GetDomainStatus(taskinfo->task_id);
        if (vm_status == VS_SHUT_OFF) {
            if (E_SUCCESS == m_vm_client.StartDomain(taskinfo->task_id)) {
                taskinfo->__set_status(TS_Running);
                taskinfo->__set_operation(T_OP_None);
                m_task_db.write_task(taskinfo);

                LOG_INFO << "restart task " << taskinfo->task_id << " successful";
            } else {
                taskinfo->__set_operation(T_OP_None);
                m_task_db.write_task(taskinfo);

                LOG_ERROR << "restart task " << taskinfo->task_id << " failed";
            }
        } else if (vm_status == VS_RUNNING) {
            if (E_SUCCESS == m_vm_client.RebootDomain(taskinfo->task_id)) {
                taskinfo->__set_status(TS_Running);
                taskinfo->__set_operation(T_OP_None);
                m_task_db.write_task(taskinfo);

                LOG_INFO << "restart task " << taskinfo->task_id << " successful";
            } else {
                taskinfo->__set_operation(T_OP_None);
                m_task_db.write_task(taskinfo);

                LOG_ERROR << "restart task " << taskinfo->task_id << " failed";
            }
        }
    } else if (taskinfo->operation == T_OP_Reset) {
        if (E_SUCCESS == m_vm_client.ResetDomain(taskinfo->task_id)) {
            taskinfo->__set_status(TS_Running);
            taskinfo->__set_operation(T_OP_None);
            m_task_db.write_task(taskinfo);

            LOG_INFO << "reset task " << taskinfo->task_id << " successful";
        } else {
            taskinfo->__set_operation(T_OP_None);
            m_task_db.write_task(taskinfo);

            LOG_ERROR << "reset task " << taskinfo->task_id << " failed";
        }
    } else if (taskinfo->operation == T_OP_Delete) {
        EVmStatus vm_status = m_vm_client.GetDomainStatus(taskinfo->task_id);
        if (vm_status == VS_SHUT_OFF) {
            if (E_SUCCESS == m_vm_client.UndefineDomain(taskinfo->task_id)) {
                delete_task_data(taskinfo->task_id);

                LOG_INFO << "delete task " << taskinfo->task_id << " successful";
            } else {
                taskinfo->__set_operation(T_OP_None);
                m_task_db.write_task(taskinfo);

                LOG_ERROR << "delete task " << taskinfo->task_id << " failed";
            }
        } else if (vm_status == VS_RUNNING) {
            if (E_SUCCESS == m_vm_client.DestoryDomain(taskinfo->task_id)) {
                sleep(1);
                if (E_SUCCESS == m_vm_client.UndefineDomain(taskinfo->task_id)) {
                    delete_task_data(taskinfo->task_id);

                    LOG_INFO << "delete task " << taskinfo->task_id << " successful";
                } else {
                    taskinfo->__set_operation(T_OP_None);
                    m_task_db.write_task(taskinfo);

                    LOG_ERROR << "delete task " << taskinfo->task_id << " failed";
                }
            } else {
                taskinfo->__set_operation(T_OP_None);
                m_task_db.write_task(taskinfo);

                LOG_ERROR << "delete task " << taskinfo->task_id << " failed";
            }
        }
    }
}

std::string req_machine_status() {
    httplib::SSLClient cli(conf_manager::instance().get_dbc_chain_domain(), 443);
    std::string str_send = R"({"jsonrpc": "2.0", "id": 1, "method":"onlineProfile_getMachineInfo", "params": [")"
            + conf_manager::instance().get_node_id() + R"("]})";
    std::shared_ptr<httplib::Response> resp = cli.Post("/", str_send, "application/json");
    if (resp != nullptr) {
        rapidjson::Document doc;
        doc.Parse(resp->body.c_str());
        if (!doc.IsObject()) {
            LOG_ERROR << "parse response failed";
            return "";
        }

        if (!doc.HasMember("result")) {
            LOG_ERROR << "parse response failed";
            return "";
        }

        const rapidjson::Value &v_result = doc["result"];
        if (!v_result.IsObject()) {
            LOG_ERROR << "parse response failed";
            return "";
        }

        if (!v_result.HasMember("machineStatus")) {
            LOG_ERROR << "parse response failed";
            return "";
        }

        const rapidjson::Value &v_machineStatus = v_result["machineStatus"];
        if (!v_machineStatus.IsString()) {
            LOG_ERROR << "parse response failed";
            return "";
        }

        std::string machine_status = v_machineStatus.GetString();
        return machine_status;
    } else {
        return "";
    }
}

int64_t req_rent_end(const std::string &wallet) {
    int64_t cur_rent_end = 0;

    httplib::SSLClient cli(conf_manager::instance().get_dbc_chain_domain(), 443);
    std::string str_send = R"({"jsonrpc": "2.0", "id": 1, "method":"rentMachine_getRentOrder", "params": [")"
            + wallet + R"(",")" + conf_manager::instance().get_node_id() + R"("]})";
    std::shared_ptr<httplib::Response> resp = cli.Post("/", str_send, "application/json");
    if (resp != nullptr) {
        do {
            rapidjson::Document doc;
            doc.Parse(resp->body.c_str());
            if (!doc.IsObject()) break;
            if (!doc.HasMember("result")) break;

            const rapidjson::Value &v_result = doc["result"];
            if (!v_result.IsObject()) break;

            if (v_result.HasMember("rentEnd")) {
                const rapidjson::Value &v_rentEnd = v_result["rentEnd"];
                if (!v_rentEnd.IsNumber()) break;

                int64_t rentEnd = v_rentEnd.GetInt64();
                if (rentEnd > 0) {
                    cur_rent_end = rentEnd;
                    break;
                }
            }
        } while (0);
    }

    return cur_rent_end;
}

int64_t req_cur_block() {
    httplib::SSLClient cli(conf_manager::instance().get_dbc_chain_domain(), 443);
    std::string str_send = R"({"jsonrpc": "2.0", "id": 1, "method":"chain_getBlock", "params": []}")";
    std::shared_ptr<httplib::Response> resp = cli.Post("/", str_send, "application/json");
    if (resp != nullptr) {
        rapidjson::Document doc;
        doc.Parse(resp->body.c_str());
        if (!doc.IsObject()) {
            return 0;
        }

        if (!doc.HasMember("result")) {
            return 0;
        }

        const rapidjson::Value &v_result = doc["result"];
        if (!v_result.IsObject()) {
            return 0;
        }

        if (!v_result.HasMember("block")) {
            return 0;
        }

        const rapidjson::Value &v_block = v_result["block"];
        if (!v_block.IsObject()) {
            return 0;
        }

        if (!v_block.HasMember("header")) {
            return 0;
        }

        const rapidjson::Value &v_header = v_block["header"];
        if (!v_header.IsObject()) {
            return 0;
        }

        if (!v_header.HasMember("number")) {
            return 0;
        }

        const rapidjson::Value &v_number = v_header["number"];
        if (!v_number.IsString()) {
            return 0;
        }

        char* p;
        int64_t n = 0;
        try {
            n = strtol(v_number.GetString(), &p, 16);
        } catch(...) {
            n = 0;
        }

        return n;
    } else {
        return 0;
    }
}

void TaskManager::PruneTask() {
    remove_reject_iptable();

    std::string machine_status = req_machine_status();
    if (machine_status.empty()) return;

    std::string url = "qemu+tcp://localhost:16509/system";
    virConnectPtr connPtr = virConnectOpen(url.c_str());
    if (nullptr == connPtr) {
        return;
    }

    for (auto& it : m_wallet_tasks) {
        if (machine_status == "AddingCustomizeInfo" || machine_status == "DistributingOrder" ||
            machine_status == "CommitteeVerifying" || machine_status == "CommitteeRefused") {
            std::vector<std::string> ids = it.second->task_ids;
            for (auto& task_id : ids) {
                if (task_id.find("vm_check_") == std::string::npos) {
                    close_task(connPtr, task_id);
                }
            }
        } else if (machine_status == "WaitingFulfill" || machine_status == "Online") {
            std::vector<std::string> ids = it.second->task_ids;
            for (auto& task_id : ids) {
                if (task_id.find("vm_check_") == std::string::npos) {
                    close_task(connPtr, task_id);
                } else {
                    close_and_delete_task(connPtr, task_id);
                }
            }
        } else if (machine_status == "creating" || machine_status == "rented") {
            std::vector<std::string> ids = it.second->task_ids;
            for (auto& task_id : ids) {
                if (task_id.find("vm_check_") != std::string::npos) {
                    close_and_delete_task(connPtr, task_id);
                }
            }
        }

        if (machine_status == "WaitingFulfill" || machine_status == "Online" ||
            machine_status == "creating" || machine_status == "rented") {
            int64_t rent_end = req_rent_end(it.first);
            if (rent_end <= 0) {
                std::vector<std::string> ids = it.second->task_ids;
                for (auto& task_id : ids) {
                    close_task(connPtr, task_id);
                }

                // 1小时出120个块
                int64_t wallet_rent_end = it.second->rent_end;
                int64_t reserve_end = wallet_rent_end + 120 * 24 * 10; //保留10天
                int64_t cur_block = req_cur_block();
                if (reserve_end > cur_block) {
                    ids = it.second->task_ids;
                    for (auto& task_id : ids) {
                        close_and_delete_task(connPtr, task_id);
                    }
                }
            } else {
                if (rent_end > it.second->rent_end) {
                    it.second->rent_end = rent_end;
                    m_wallet_task_db.write_data(it.second);
                }
            }
        }
    }

    virConnectClose(connPtr);
}

/*
int32_t task_scheduling::init(bpo::variables_map& options) {
    int32_t ret = E_SUCCESS;

    m_container_worker = std::make_shared<container_worker>();
    ret = m_container_worker->init();
    if (E_SUCCESS != ret) {
        LOG_ERROR << "ai power provider service init container worker error";
        return ret;
    }

    m_vm_worker = std::make_shared<vm_worker>();
    ret = m_vm_worker->init();
    if (E_SUCCESS != ret) {
        LOG_ERROR << "ai power provider service init vm worker error";
        return ret;
    }

    if (E_SUCCESS != init_db("prov_training_task.db")) {
        LOG_ERROR << "init prov_training_task.db failed!";
        return E_DEFAULT;
    }

    m_pull_image_mng = std::make_shared<image_manager>();

    if (options.count("ai_training") > 0) {
        m_is_computing_node = true;
        gpu_pool_helper::update_gpu_from_proc(m_gpu_pool, "/proc/driver/nvidia/gpus");
    }

    m_prune_intervel = CONF_MANAGER->get_prune_task_stop_interval();
    if (m_prune_intervel < 1 || m_prune_intervel > 8760) {
        return E_DEFAULT;
    }

    return E_SUCCESS;
}

int32_t task_scheduling::init_db(const std::string& db_name) {
    int32_t ret = m_task_db.init_db(env_manager::get_db_path(), db_name);
    if (E_SUCCESS != ret) {
        LOG_ERROR << "init_db failed!";
        return ret;
    }

    ret = m_task_db.load_user_task(m_training_tasks);
    if (ret != E_SUCCESS) {
        LOG_ERROR << "load user task failed!";
        return ret;
    }

    for (const auto& item : m_training_tasks) {
        auto task = item.second;
        if (task_status_queueing == task->status
            || task_status_pulling_image == task->status
            || task_status_creating_image == task->status) {
            m_queueing_tasks.push_back(task);
        }
        else if (task_status_running == task->status) {
            if (!m_gpu_pool.allocate(task->gpus)) {
                LOG_ERROR << "out of gpu resource, " << "task id: " << task->task_id
                    << ", gpu requirement " << task->gpus << ", gpu state " << m_gpu_pool.toString();

                stop_task(task, task_status_out_of_gpu_resource);
            }

            m_running_tasks[task->task_id] = task;
        }
    }

    m_queueing_tasks.sort(
        [](const std::shared_ptr<ai_training_task>& t1, const std::shared_ptr<ai_training_task>& t2) {
            return t1->received_time_stamp < t2->received_time_stamp;
        });

    return E_SUCCESS;
}

// env.NVIDIA_VISIBLE_DEVICES
std::string task_scheduling::get_gpu_spec(const std::string& s) {
    if (s.empty()) {
        return "";
    }

    std::string rt;
    std::stringstream ss;
    ss << s;
    boost::property_tree::ptree pt;

    try {
        boost::property_tree::read_json(ss, pt);
        rt = pt.get<std::string>("env.NVIDIA_VISIBLE_DEVICES");
        if (!rt.empty()) {
            matrix::core::string_util::trim(rt);
        }
    }
    catch (...) {
        return "";
    }

    return rt;
}

// uname: 宿主机已存在的用户名
std::string genpasswd(const char* uname, const char* pwd) {
    if (geteuid() != 0) {
        fprintf(stderr, "must be setuid root");
        return "";
    }

    struct spwd* shd = getspnam(uname);
    if (shd != nullptr) {
        static char crypt_char[1024] = { 0 };
        strcpy(crypt_char, shd->sp_pwdp);
        char salt[1024] = { 0 };
        int i = 0, j = 0;
        while (shd->sp_pwdp[i] != '\0') {
            salt[i] = shd->sp_pwdp[i];
            if (salt[i] == '$') {
                j++;
                if (j == 3) {
                    salt[i + 1] = '\0';
                    break;
                }
            }

            i++;
        }

        if (j < 3) {
            printf("file error or user cannot use.");
            return "";
        }

        std::string crypt_pwd = crypt(pwd, salt);
        //printf("salt: %s, crypt: %s\n", salt, crypt_pwd.c_str());
        //printf("shadowd passwd: %s\n", shd->sp_pwdp);
        return crypt_pwd;
    }
    else {
        printf("getspnam return nullptr: %d, %s\n", errno, strerror(errno));
        return "";
    }
}

// username1: 宿主机已经存在的用户名
// username2: 虚拟机的用户名
bool set_vm_passwd(const std::string& vm_name, const std::string& username1, const std::string& username2,
    const std::string& pwd) {
    //连接到虚拟机监控程序(Hypervisor)
    virConnectPtr conn_ptr = virConnectOpen("qemu+tcp://localhost/system");
    if (conn_ptr == nullptr) {
        virErrorPtr error = virGetLastError();
        printf("connect virt error: %s\n", error->message);
        virFreeError(error);
        return false;
    }

    virDomainPtr domain_ptr = virDomainLookupByName(conn_ptr, vm_name.c_str());
    if (domain_ptr == nullptr) {
        virErrorPtr error = virGetLastError();
        printf("virDomainLookupByName error: %s\n", error->message);
        virFreeError(error);
        virConnectClose(conn_ptr);
        return false;
    }

    std::string crypt_pwd = genpasswd(username1.c_str(), pwd.c_str());
    if (!crypt_pwd.empty()) {
        virDomainSetUserPassword(domain_ptr, username2.c_str(), crypt_pwd.c_str(),
            VIR_DOMAIN_PASSWORD_ENCRYPTED);
        LOG_INFO << "set_domain_pwd: " << username2 << ":" << pwd;
        return true;
    }
    else {
        LOG_INFO << "set_domain_pwd: " << username2 << ":" << pwd;
        return false;
    }
}

int32_t task_scheduling::process_task() {
    for (auto& it : m_running_tasks) {
        std::shared_ptr<ai_training_task> task = it.second;
        if (task->set_pwd == 0) {
            if (!task->pwd.empty()) {
                std::string cmd = "virsh set-user-password ";
                cmd += task->task_id + " ubuntu " + task->pwd;
                std::string result = run_shell(cmd.c_str());
                task->set_pwd = 1;
                LOG_INFO << cmd << ", result=" << result;

                //bool set_success = set_vm_passwd(task->task_id, "dbc", "ubuntu", task->pwd);
                //if (set_success) {
                //	task->set_pwd = 1;
                //	LOG_INFO << "set_pwd:" << task->pwd;
                //}
            }
        }
    }

    if (!m_queueing_tasks.empty()) {
        auto task = m_queueing_tasks.front();

        if (task_status_queueing == task->status || task_status_creating_image == task->status) {
            return exec_task(task);
        }
        else if (task_status_pulling_image == task->status) {
            return check_pull_image_state(task);
        }
        else {
            m_queueing_tasks.pop_front();
        }
    }

    for (auto& it : m_running_tasks) {
        check_training_task_status(it.second);
    }

    return E_SUCCESS;
}

int32_t task_scheduling::exec_task(const std::shared_ptr<ai_training_task>& task) {
    bool is_docker = false;
    if (task->server_specification.find("docker") != std::string::npos)
        is_docker = true;

    if (task->error_times > AI_TRAINING_MAX_RETRY_TIMES) {
        stop_task(task, task_abnormally_closed);
        LOG_WARNING << "ai power provider service restart container/vm too many times and close task, "
            << "task id: " << task->task_id << " container id: " << task->container_id;
        return E_DEFAULT;
    }

    // restart
    if (task->server_specification == "restart_docker" || task->server_specification == "restart_vm") {
        int32_t ret = restart_task(task, task->server_specification == "restart_docker");
        m_queueing_tasks.remove(task);
        if (ret != E_SUCCESS) {
            task->__set_status(task_status_stopped);
            m_task_db.write_task_to_db(task);
            LOG_ERROR << "restart user task failed, task id:" << task->task_id;
            return E_DEFAULT;
        }
        else {
            m_running_tasks[task->task_id] = task;
            m_gpu_pool.allocate(task->gpus);
            m_task_db.write_task_to_db(task);
            LOG_INFO << "restart user task success, task id:" << task->task_id;
            return E_SUCCESS;
        }
    }

    // update
    std::string operation = m_container_worker->get_operation(task);

    if (operation == "update") {
        auto old_task = m_running_tasks[task->task_id];
        if (nullptr == old_task) {
            if (!m_gpu_pool.check(get_gpu_spec(task->server_specification))) {
                LOG_ERROR << "update out of gpu resource, " << "task id: " << task->task_id
                    << ", gpu requirement "
                    << task->gpus << ", gpu remainder " << m_gpu_pool.toString();
                task->__set_status(task_status_update_error);

                m_task_db.write_task_to_db(task);
                return E_DEFAULT;
            }
        }
        else {
            std::string old_gpus = old_task->gpus;
            m_gpu_pool.free(old_gpus);
            if (!m_gpu_pool.check(get_gpu_spec(task->server_specification))) {
                LOG_ERROR << "update out of gpu resource, " << "task id: " << task->task_id
                    << ", gpu requirement "
                    << task->gpus << ", gpu remainder " << m_gpu_pool.toString();
                task->__set_status(task_status_update_error);
                m_gpu_pool.allocate(old_gpus);//add old gpus again
                m_task_db.write_task_to_db(task);
                return E_DEFAULT;
            }
            m_gpu_pool.allocate(old_gpus);
            LOG_INFO << "task will update, allocate m_gpu_pool:" << m_gpu_pool.toString();
        }

        int ret = change_gpu_id(task);

        if (task->status != task_status_creating_image) {
            m_queueing_tasks.remove(task);
        }
        else {
            LOG_INFO << "task_status_creating_image:" << task->task_id;
        }

        if (ret == E_SUCCESS) {
            if (task->status == task_status_creating_image) {
                LOG_INFO << "creating image:" << task->task_id;
                task->__set_status(task_status_creating_image);
                m_queueing_tasks.remove(task);
                m_queueing_tasks.push_back(task);
                return E_DEFAULT;
            }

            task->__set_status(task_status_running);
            if (nullptr != old_task) {
                std::string old_gpus = old_task->gpus;
                m_gpu_pool.free(old_gpus);
            }

            m_gpu_pool.allocate(task->gpus);
            LOG_INFO << "gpu state " << m_gpu_pool.toString();
            m_running_tasks[task->task_id] = task;
            m_training_tasks[task->task_id] = task;
            m_task_db.write_task_to_db(task);

            return E_SUCCESS;
        }
        else {
            LOG_ERROR << "user task task_status_update_error, start inspect_container container id: "
                << task->container_id;
            std::shared_ptr<container_inspect_response> resp = CONTAINER_WORKER_IF->inspect_container(
                task->container_id);
            if (nullptr == resp) {
                LOG_ERROR << "user task check container error, container id: " << task->container_id;
                m_running_tasks.erase(task->task_id);
            }
            else if (resp->state.running) {
                auto ori_task = find_task(task->task_id);
                ori_task->__set_status(task_status_running);
                m_running_tasks[ori_task->task_id] = ori_task;
                m_training_tasks[ori_task->task_id] = ori_task;
                m_task_db.write_task_to_db(ori_task);
            }
            else {
                m_running_tasks.erase(task->task_id);
            }

            return E_DEFAULT;
        }
    }
    else {
        if (task_status_queueing == task->status) {
            if (!m_gpu_pool.check(task->gpus)) {
                LOG_ERROR << "out of gpu resource, " << "task id: " << task->task_id << ", gpu requirement "
                    << task->gpus << ", gpu remainder " << m_gpu_pool.toString();
                stop_task(task, task_status_out_of_gpu_resource);
                return E_DEFAULT;
            }
        }

        int ret = start_task(task, is_docker);

        if (task->status != task_status_pulling_image) {
            m_queueing_tasks.remove(task);
        }

        if (ret != E_SUCCESS) {
            task->error_times++;
            switch (ret) {
            case E_NO_DISK_SPACE:
                stop_task(task, task_nospace_closed);
                break;
            case E_PULLING_IMAGE:
            case E_IMAGE_NOT_FOUND:
                stop_task(task, task_noimage_closed);
                break;
            default:
                m_task_db.write_task_to_db(task);
                break;
            }

            LOG_ERROR << "start user task failed, task id:" << task->task_id;
            return E_DEFAULT;
        }
        else {
            if (task->status == task_status_running) {
                //jimmy: move task from waiting queue  into running tasks map
                LOG_INFO << "move task from waiting queue  into running tasks map" << task->task_id;
                m_running_tasks[task->task_id] = task;

                if (!m_gpu_pool.allocate(task->gpus)) {
                    // is supposed never happen because gpu check passed before
                    LOG_INFO << "out of gpu resource, " << "task id: " << task->task_id << ", gpu requirement "
                        << task->gpus << ", gpu remainder " << m_gpu_pool.toString();
                    stop_task(task, task_status_out_of_gpu_resource);
                    return E_DEFAULT;
                }
                else {
                    LOG_INFO << "gpu state " << m_gpu_pool.toString();
                }

                return E_SUCCESS;
            }

            m_task_db.write_task_to_db(task);
            return E_DEFAULT;
        }
    }
}

int32_t task_scheduling::check_pull_image_state(const std::shared_ptr<ai_training_task>& task) {
    if (task == nullptr) {
        return E_NULL_POINTER;
    }

    bool is_docker = false;
    if (task->server_specification.find("docker") != std::string::npos)
        is_docker = true;

    if (is_docker) {
        if (m_pull_image_mng->get_pull_image_name() == task->training_engine) {
            //cost time too long to pull image.
            if ((time_util::get_time_stamp_ms() - m_pull_image_mng->get_start_time()) >=
                AI_PULLING_IMAGE_TIMER_INTERVAL) {
                //pull error
                LOG_ERROR << "pull image too long." << " engine image:" << task->training_engine;
                stop_task(task, task_noimage_closed);
                return E_PULLING_IMAGE;
            }
            else {
                if (PULLING == m_pull_image_mng->check_pull_state()) {
                    LOG_DEBUG << "pulling: docker pull " << task->training_engine;
                    return E_SUCCESS;
                }
                else {
                    //docker is not pulling image.
                    if (CONTAINER_WORKER_IF->exist_docker_image(task->training_engine, 20) != E_SUCCESS) {
                        LOG_WARNING << "ai power provider service pull image failed";
                        stop_task(task, task_noimage_closed);
                        return E_PULLING_IMAGE;
                    }

                    task->__set_status(task_status_queueing);
                }
            }
        }
        else {
            //task state is pulling, but image is not pulling. so dbc may be restarted.
            task->__set_status(task_status_queueing);
        }
    }
    else {
        //todo: vm

    }

    return E_SUCCESS;
}

int32_t task_scheduling::check_training_task_status(const std::shared_ptr<ai_training_task>& task) {
    if (task == nullptr) {
        return E_NULL_POINTER;
    }

    bool is_docker = false;
    if (task->server_specification.find("docker") != std::string::npos)
        is_docker = true;

    if (is_docker) {
        if (task->container_id.empty()) {
            return E_NULL_POINTER;
        }

        //inspect container
        std::shared_ptr<container_inspect_response> resp = CONTAINER_WORKER_IF->inspect_container(
            task->container_id);
        if (nullptr == resp) {
            LOG_ERROR << "user task check container error, container id: " << task->container_id;
            return E_DEFAULT;
        }
        else {
            task->error_times = 0;
            m_task_db.write_task_to_db(task);
        }

        if (resp->state.running) {
            return E_SUCCESS;
        }
        else if (0 != resp->state.exit_code && 137 != resp->state.exit_code &&
            task->status == task_status_running) {
            LOG_INFO << "inspect container not running, " << "task id: " << task->task_id << " container id: "
                << task->container_id << " exit_code" << resp->state.exit_code;

            start_task(task, is_docker);
            return E_SUCCESS;
        }
    }
    else {
        //todo: vm
    }

    return E_SUCCESS;
}

void task_scheduling::update_gpu_info_from_proc() {
    if (m_is_computing_node) {
        gpu_pool gp;
        gpu_pool_helper::update_gpu_from_proc(gp, "/proc/driver/nvidia/gpus");
        m_gpu_pool.merge(gp);
    }
}

int32_t task_scheduling::stop_task(std::shared_ptr<ai_training_task> task, training_task_status end_status) {
    // case 1: stop a task from running set
    if (task->status == task_status_running || task->status == task_status_update_error || task->status > task_status_stopped ||
        task->status == task_status_queueing) {
        int32_t ret = this->stop_task(task);
        if (E_SUCCESS != ret) {
            LOG_ERROR << "stop container error, container id: " << task->container_id << " task is:"
                << task->task_id;
        }
        else {
            LOG_INFO << "stop container success, container id: " << task->container_id << " task is:"
                << task->task_id << " end time: " << task->end_time;
        }

        // task->__set_end_time(time_util::get_time_stamp_ms());
        task->__set_status(end_status);
        // m_task_db.write_task_to_db(task);

        m_running_tasks.erase(task->task_id);

        if (task_status_out_of_gpu_resource != end_status) {
            //free gpu resource
            m_gpu_pool.free(task->gpus);
        }
        task->__set_end_time(time_util::get_time_stamp_ms());

        m_task_db.write_task_to_db(task);
        return E_SUCCESS;
    }

    // case 2: stop a task from waiting queue
    if (m_queueing_tasks.empty()) {
        return E_SUCCESS;
    }
    auto top_task = m_queueing_tasks.front();
    m_queueing_tasks.remove(task);

    if (task_status_pulling_image == task->status) {
        stop_pull_image(task);
    }

    task->__set_end_time(time_util::get_time_stamp_ms());
    task->__set_status(end_status);
    m_task_db.write_task_to_db(task);

    if (end_status != task_overdue_closed) {
        //if task is not the top task, means the task is have never been scheduled.
        //At this time, the task is not needed to report task status to oss..
        if (task->task_id != top_task->task_id) {
            LOG_DEBUG << "task is not the top task, do not need report status to oss. task is: "
                << task->task_id;
            return E_SUCCESS;
        }
        LOG_INFO << "dbc close task, report the event to oss system." << " task:" << task->task_id << " status:"
            << to_training_task_status_string(end_status);
        return E_SUCCESS;
    }

    return E_SUCCESS;
}

int32_t task_scheduling::add_update_task(std::shared_ptr<ai_training_task> task) {
    m_queueing_tasks.remove(task);
    m_queueing_tasks.push_back(task);
    return E_SUCCESS;
}

int32_t task_scheduling::add_task(const std::shared_ptr<ai_training_task>& task) {
    if (m_training_tasks.size() > MAX_TASK_COUNT) {
        LOG_ERROR << "task is full.";
        return E_DEFAULT;
    }

    if (E_SUCCESS != m_task_db.write_task_to_db(task)) {
        return E_DEFAULT;
    }

    m_queueing_tasks.push_back(task);
    m_training_tasks[task->task_id] = task;

    return E_SUCCESS;
}

std::shared_ptr<ai_training_task> task_scheduling::find_task(std::string task_id) {
    auto it_task = m_training_tasks.find(task_id);
    if (it_task != m_training_tasks.end()) {
        return it_task->second;
    }

    return nullptr;
}

int32_t task_scheduling::prune_task() {
    prune_task(m_prune_intervel);
    return E_SUCCESS;
}

//  This method will check all stopped user tasks and delete them from db and cache.
//  Docker container will be removed by common_service::on_timer_prune_container.
// @param interval, threshold that system will wait before deleting a stoppped task.
int32_t task_scheduling::prune_task(int16_t interval) {
    int64_t cur = time_util::get_time_stamp_ms();

    int64_t p_interval = (int64_t)interval * 3600 * 1000;

    LOG_INFO << "prune docker container." << " interval:" << interval << "h";

    for (auto task_iter = m_training_tasks.begin(); task_iter != m_training_tasks.end();) {
        if (task_iter->second->status < task_status_stopped) {
            task_iter++;
            continue;
        }

        // container_id is empty when the task fail to exectue.
        // note: client node may fail to update its task status after task pruned from the computing node.
        bool enable_delete = false;
        auto task = task_iter->second;
        if ((cur - task->end_time) > p_interval) {
            enable_delete = true;
            LOG_INFO << "prune long duration stopped task " << task->task_id;
        }
        else if (task->container_id.empty()) {
            enable_delete = true;
            LOG_INFO << "prune fail to exeucte: " << task->task_id;
        }

        if (enable_delete) {
            m_task_db.delete_task(task);
            m_training_tasks.erase(task_iter++);
        }
        else {
            task_iter++;
        }
    }

    if (0 == interval) {
        return E_SUCCESS;
    }

    if (m_training_tasks.size() > MAX_PRUNE_TASK_COUNT) {
        prune_task(interval / 2);
    }

    return E_SUCCESS;
}

int32_t task_scheduling::process_reboot_task(std::shared_ptr<ai_training_task> task) {
    if (task == nullptr) return E_DEFAULT;

    if (task->code_dir == NODE_REBOOT) {
        // reboot node
        LOG_WARNING << "node reboot now";
        system("wall \"System will reboot by dbc in one minute!\"");
        system("sudo /sbin/shutdown -r +1");
        return E_SUCCESS;
    }

    return E_SUCCESS;
}

std::string task_scheduling::get_gpu_state() {
    return m_gpu_pool.toString();
}

std::string task_scheduling::get_active_tasks() {
    std::string s;
    for (auto& kvp : m_running_tasks) {
        if (s.empty()) {
            s = "[";
        }
        else {
            s += ",";
        }

        s += "\"" + kvp.first + "\"";
    }

    if (s.empty()) {
        s = "[]";
    }
    else {
        s += "]";
    }
    return s;
}

int32_t task_scheduling::update_task(std::shared_ptr<ai_training_task> task) {
    if (nullptr == task) {
        return E_DEFAULT;
    }
    if (task->entry_file.empty() || task->code_dir.empty() || task->task_id.empty()) {
        LOG_DEBUG << "task config error.";
        return E_DEFAULT;
    }

    //  auto state = get_task_state(task);

    if (task->container_id.empty()) {

        LOG_INFO << "container_id null. container_id";

        return E_DEFAULT;
    }

    std::shared_ptr<update_container_config> config = m_container_worker->get_update_container_config(task);
    int32_t ret = CONTAINER_WORKER_IF->update_container(task->container_id, config);


    if (ret != E_SUCCESS) {
        LOG_ERROR << "update task error. Task id:" << task->task_id;
        return E_DEFAULT;
    }

    LOG_INFO << "update task success. Task id:" << task->task_id;
    //                task->__set_update_time(time_util::get_time_stamp_ms());
    task->__set_start_time(time_util::get_time_stamp_ms());
    task->__set_status(task_status_running);
    task->error_times = 0;

    //             task->__set_memory(config->memory);
    //             task->__set_memory_swap(config->memory_swap);
    // task->__set_gpus(config->env);
    m_task_db.write_task_to_db(task);


    return E_SUCCESS;
}

int32_t task_scheduling::update_task_commit_image(std::shared_ptr<ai_training_task> task) {
    if (nullptr == task) {
        return E_DEFAULT;
    }

    if (task->task_id.empty()) {
        LOG_DEBUG << "task config error.";
        task->__set_status(task_status_update_error);
        task->error_times = 0;

        return E_DEFAULT;
    }


    std::string autodbcimage_version = m_container_worker->get_autodbcimage_version(task);
    LOG_INFO << "autodbcimage_version" << autodbcimage_version;
    if (autodbcimage_version.empty()) {
        task->__set_status(task_status_update_error);
        task->error_times = 0;

        return E_DEFAULT;
    }

    std::string training_engine_name =
        "www.dbctalk.ai:5000/dbc-free-container:autodbcimage_" + task->task_id.substr(0, 6) + "_" +
        task->container_id.substr(0, 6) +
        autodbcimage_version;
    std::string image_id = "";
    bool can_create_container = false;
    LOG_INFO << "task->status:" << task->status;
    LOG_INFO << "training_engine_name 1 " << training_engine_name;
    if (task->status != task_status_creating_image) { //刚开始创建

        std::shared_ptr<container_inspect_response> resp = CONTAINER_WORKER_IF->inspect_container(
            task->container_id);//需要查询一下task中的镜像名字是否和真正的容器id一致

        if (resp == nullptr) //说明之前创建新的容器出问题了，没有保存container_id
        {
            std::string container_id = CONTAINER_WORKER_IF->get_container_id_current(task->task_id);
            if (container_id.empty()) {
                container_id = CONTAINER_WORKER_IF->get_container_id_current(task->task_id);
            }

            if (!container_id.empty()) {
                resp = CONTAINER_WORKER_IF->inspect_container(container_id);
            }

            if (resp == nullptr) {
                return E_DEFAULT;
            }

            task->__set_container_id(container_id);
        }
        training_engine_name =
            "www.dbctalk.ai:5000/dbc-free-container:autodbcimage_" + task->task_id.substr(0, 6) + "_" +
            task->container_id.substr(0, 6) +
            autodbcimage_version;
        LOG_INFO << "training_engine_name 2 " << training_engine_name;

        if (E_SUCCESS == CONTAINER_WORKER_IF->exist_docker_image(training_engine_name, 30)) {

            LOG_INFO << "delete the same name image";
            CONTAINER_WORKER_IF->delete_image(training_engine_name);//删除之前的同名镜像
            sleep(10);
        }
        int64_t sleep_time = m_container_worker->get_sleep_time(task);
        task->__set_start_time(time_util::get_time_stamp_ms());
        LOG_INFO << "sleep_time waiting :" << sleep_time << "s";
        task->__set_status(task_status_creating_image);
        std::string status = CONTAINER_WORKER_IF->commit_image(task->container_id, autodbcimage_version,
            task->task_id, 60);
        if (status.compare("error") == 0) {
            return E_DEFAULT;
        }
        return E_SUCCESS;

    }
    else if (task->status == task_status_creating_image) { //正在创建中

        if (E_SUCCESS == CONTAINER_WORKER_IF->exist_docker_image(training_engine_name, 60)) {
            LOG_INFO << "is  exist_docker_image";
            //  sleep(100);

            can_create_container = true;


        }
        else {
            int64_t real_time = time_util::get_time_stamp_ms();
            int64_t sleep_time = m_container_worker->get_sleep_time(task);
            int64_t sub_time = real_time - task->start_time;

            LOG_INFO << "real_time  ：" << real_time;
            LOG_INFO << "task  start_time：" << task->start_time;
            LOG_INFO << "sub_time：" << sub_time;

            if (sub_time > sleep_time * 1000) {//是否创建时间已经超过sleep_time
                //     if(sub_time>sleep_time*10){//测试
                can_create_container = false;

            }
            else {
                return E_SUCCESS;
            }

        }


    }


    if (can_create_container) {
        std::string training_engine_original = task->training_engine;
        //   training_engine_new="www.dbctalk.ai:5000/dbc-free-container:autodbcimage_"+task->task_id.substr(0,6)+"_"+task->container_id.substr(0,6)+autodbcimage_version;
        task->__set_training_engine(training_engine_name);
        LOG_INFO << "training_engine_original:" << training_engine_original;
        LOG_INFO << "training_engine_new:"
            << "www.dbctalk.ai:5000/dbc-free-container:autodbcimage_" + task->task_id.substr(0, 6) + "_" +
            task->container_id.substr(0, 6) + autodbcimage_version;
        int32_t status = start_task_from_new_image(task, autodbcimage_version, training_engine_name);

        if (status != E_NO_START_CONTAINER && status != E_SUCCESS) {
            task->__set_training_engine(training_engine_original);
            return E_DEFAULT;
        }


    }
    else {

        //  CONTAINER_WORKER_IF->delete_image(image_id);//delete new image,防止可能创建成功
        LOG_INFO << "task_status_update_error";
        task->__set_status(task_status_update_error);
        task->error_times = 0;

        return E_DEFAULT;
    }

    task->__set_gpus(get_gpu_spec(task->server_specification));
    return E_SUCCESS;
}

int32_t task_scheduling::change_gpu_id(std::shared_ptr<ai_training_task> task) {
    if (nullptr == task) {
        return E_DEFAULT;
    }

    if (task->task_id.empty()) {
        LOG_DEBUG << "task config error.";
        task->__set_status(task_status_update_error);
        return E_DEFAULT;
    }

    if (task->status != task_status_creating_image) {
        std::shared_ptr<container_inspect_response> resp = CONTAINER_WORKER_IF->inspect_container(
            task->container_id);//需要查询一下task中的镜像名字是否和真正的容器id一致
        if (resp == nullptr) //说明之前创建新的容器出问题了，没有保存container_id
        {
            std::string container_id = CONTAINER_WORKER_IF->get_container_id_current(task->task_id);
            if (container_id.empty()) {
                container_id = CONTAINER_WORKER_IF->get_container_id_current(task->task_id);
            }

            if (!container_id.empty()) {
                resp = CONTAINER_WORKER_IF->inspect_container(container_id);
            }

            if (resp == nullptr) {
                task->__set_status(task_status_update_error);
                return E_DEFAULT;
            }

            task->__set_container_id(container_id);
        }

        std::string change_gpu_id_file_name =
            env_manager::get_home_path().generic_string() + "/tool/change_gpu_id.sh";
        std::string task_id = task->task_id;

        std::string old_gpu_id = m_container_worker->get_old_gpu_id(task);
        std::string new_gpu_id = m_container_worker->get_new_gpu_id(task);

        std::string old_cpu_shares = std::to_string(m_container_worker->get_old_cpu_shares(task));
        std::string new_cpu_shares = std::to_string(m_container_worker->get_new_cpu_shares(task));
        std::string cpu_shares = old_cpu_shares + "," + new_cpu_shares;

        std::string old_cpu_quota = std::to_string(m_container_worker->get_old_cpu_quota(task));
        std::string new_cpu_quota = std::to_string(m_container_worker->get_new_cpu_quota(task));
        std::string cpu_quota = old_cpu_quota + "," + new_cpu_quota;

        std::string old_memory = m_container_worker->get_old_memory(task);
        std::string new_memory = m_container_worker->get_new_memory(task);
        std::string memory = old_memory + "," + new_memory;

        std::string old_memory_swap = m_container_worker->get_old_memory_swap(task);
        std::string new_memory_swap = m_container_worker->get_new_memory_swap(task);
        std::string memory_swap = old_memory_swap + "," + new_memory_swap;

        std::string docker_dir = CONTAINER_WORKER_IF->get_docker_dir(task->container_id);
        if (docker_dir.empty()) {
            LOG_ERROR << "docker_dir is empty";
            task->__set_status(task_status_update_error);
            return E_DEFAULT;
        }
        std::string container_id = task->container_id;
        std::string m_change_gpu_id_cmd = "";

        int32_t reslut = commit_change_gpu_id_bash(change_gpu_id_file_name, task_id, old_gpu_id, new_gpu_id,
            container_id, cpu_shares,
            cpu_quota, memory, memory_swap, docker_dir);

        if (reslut == E_DEFAULT) {
            task->__set_status(task_status_update_error);
            return E_DEFAULT;
        }

        LOG_INFO << "sleep_time waiting";
        int64_t sleep_time = m_container_worker->get_sleep_time(task);
        task->__set_start_time(time_util::get_time_stamp_ms());
        LOG_INFO << "sleep_time waiting :" << sleep_time << "s";
        task->__set_status(task_status_creating_image);
        return E_SUCCESS;

    }
    else if (task->status == task_status_creating_image) { //正在创建中

        std::shared_ptr<container_inspect_response> resp = CONTAINER_WORKER_IF->inspect_container(
            task->container_id);
        if (resp != nullptr && true == resp->state.running) {

            std::string sub_task_id = task->task_id.substr(0, 12);
            std::string gpu_id = CONTAINER_WORKER_IF->get_gpu_id(sub_task_id);
            if (gpu_id.find("NVIDIA_VISIBLE_DEVICES=") == string::npos) { //如果gpu id 没有修改过来
                LOG_INFO << "task_status_update_error";
                task->__set_status(task_status_update_error);

                return E_DEFAULT;
            }
            else {

                std::vector<std::string> list;
                string_util::split(gpu_id, "=", list);
                std::string new_gpu_id = m_container_worker->get_new_gpu_id(task);
                if (new_gpu_id.compare(list[1]) != 0) {//说明没有设置成功
                    LOG_INFO << "task_status_update_error new_gpu_id !=inspect " << gpu_id;
                    task->__set_status(task_status_update_error);

                    return E_DEFAULT;
                }
            }

        }
        else {
            int64_t real_time = time_util::get_time_stamp_ms();
            int64_t sleep_time = m_container_worker->get_sleep_time(task);
            int64_t sub_time = real_time - task->start_time;

            LOG_INFO << "real_time  ：" << real_time;
            LOG_INFO << "task  start_time：" << task->start_time;
            LOG_INFO << "sub_time：" << sub_time;

            if (sub_time > 360 * 1000) {//是否创建时间已经超过sleep_time

                LOG_INFO << "task_status_update_error ：can not start container  ";
                task->__set_status(task_status_update_error);
                return E_DEFAULT;

            }

            return E_SUCCESS;
        }
    }

    task->__set_gpus(get_gpu_spec(task->server_specification));

    task->__set_status(task_status_running);

    return E_SUCCESS;
}

int32_t task_scheduling::commit_change_gpu_id_bash(std::string change_gpu_id_file_name, std::string task_id,
    std::string old_gpu_id,
    std::string new_gpu_id, std::string container_id,
    std::string cpu_shares,
    std::string cpu_quota, std::string memory,
    std::string memory_swap,
    std::string docker_dir) {
    std::string m_change_gpu_id_cmd = boost::str(boost::format("/bin/bash %s %s %s %s %s %s %s %s %s %s")
        % change_gpu_id_file_name % task_id % old_gpu_id % new_gpu_id %
        container_id % cpu_shares %
        cpu_quota
        % memory % memory_swap % docker_dir);

    LOG_INFO << "m_change_gpu_id_cmd " << m_change_gpu_id_cmd;
    LOG_INFO << " m_change_gpu_id_cmd  will commit";
    if (m_change_gpu_id_cmd.empty()) {
        LOG_ERROR << "m_change_gpu_id_cmd command is empty";
        return E_DEFAULT;
    }

    if (!std::regex_match(new_gpu_id, std::regex("[0-9]{0,}[,0-9]{0,}")) &&
        !std::regex_match(new_gpu_id, std::regex("none"))) {
        LOG_ERROR << "new_gpu_id is error ";
        return E_DEFAULT;
    }
    if (!std::regex_match(cpu_shares, std::regex("[0-9]{1,},[0-9]{1,}"))) {
        LOG_ERROR << "cpu_shares is error ";
        return E_DEFAULT;
    }
    if (!std::regex_match(cpu_quota, std::regex("[0-9]{1,},[0-9]{1,}"))) {
        LOG_ERROR << "cpu_quota is error ";
        return E_DEFAULT;
    }
    if (!std::regex_match(memory, std::regex("[0-9]{1,},[0-9]{1,}"))) {
        LOG_ERROR << "memory is error ";
        return E_DEFAULT;
    }
    if (!std::regex_match(memory_swap, std::regex("[0-9]{1,},[0-9]{1,}"))) {
        LOG_ERROR << "memory_swap is error ";
        return E_DEFAULT;
    }
    try {
        std::error_code ec;
        std::future<std::string> fut;
        std::future<std::string> fut_err;
        int32_t ret = bp::system(bp::cmd = m_change_gpu_id_cmd, bp::std_out > fut, bp::std_err > fut_err, ec);
        std::string m_change_gpu_id_cmd_log = "m_change_gpu_id_cmd info.";
        if (ec) {
            m_change_gpu_id_cmd_log += ec.message();
        }

        if (fut.valid()) {
            m_change_gpu_id_cmd_log += fut.get();
        }
        if (fut_err.valid()) {
            m_change_gpu_id_cmd_log += fut_err.get();
        }

        LOG_INFO << " m_change_gpu_id_cmd ret code:" << ret << ". ";


    }
    catch (const std::exception& e) {
        LOG_ERROR << "m_change_gpu_id_cmd error" << e.what();
        return E_DEFAULT;
    }
    catch (...) {
        LOG_ERROR << "m_change_gpu_id_cmd error";
        return E_DEFAULT;
    }

    return E_SUCCESS;
}

int32_t task_scheduling::start_task_from_new_image(std::shared_ptr<ai_training_task> task,
    std::string autodbcimage_version,
    std::string training_engine_new) {
    if (nullptr == task) {
        return E_DEFAULT;
    }

    std::shared_ptr<container_inspect_response> resp = CONTAINER_WORKER_IF->inspect_container(
        task->container_id);
    std::string old_container_id = task->container_id;
    if (resp == nullptr) //说明之前创建新的容器出问题了，没有保存container_id
    {
        std::string container_id = CONTAINER_WORKER_IF->get_container_id_current(task->task_id);
        if (container_id.empty()) {
            container_id = CONTAINER_WORKER_IF->get_container_id_current(task->task_id);
        }

        if (!container_id.empty()) {
            resp = CONTAINER_WORKER_IF->inspect_container(container_id);
        }

        if (resp == nullptr) {
            return E_DEFAULT;
        }
        old_container_id = container_id;
        task->__set_container_id(container_id);
    }

    bool original_status_container = false;//默认容器是关闭的状态
    if (true == resp->state.running) {
        original_status_container = true;
        if (E_SUCCESS == CONTAINER_WORKER_IF->stop_container(old_container_id)) {
            LOG_INFO << "stop container success , task id:" << old_container_id;


        }
        else {
            LOG_INFO << "stop container failure , task id:" << task->task_id;
            task->__set_status(task_status_update_error);
            task->error_times = 0;
            //  m_task_db.write_task_to_db(task);
            return E_DEFAULT;

        }
    }


    if (E_SUCCESS != create_task_from_image(task, autodbcimage_version)) {
        LOG_ERROR << "create task error";
        CONTAINER_WORKER_IF->delete_image(training_engine_new);//delete new image
        if (original_status_container) {
            CONTAINER_WORKER_IF->start_container(task->container_id);//start original container_id
        }

        task->__set_status(task_status_update_error);
        task->error_times = 0;
        // m_task_db.write_task_to_db(task);
        return E_DEFAULT;
    }
    else {
          //    std::string image_id=CONTAINER_WORKER_IF->get_image_id(old_container_id);

             // if(image_id.compare("error")==0){
                //  CONTAINER_WORKER_IF->remove_container(task->container_id);//delete new docker
                //  CONTAINER_WORKER_IF->delete_image(training_engine_new);//delete new image
                //  CONTAINER_WORKER_IF->start_container(old_container_id);//start original container_id
                //  task->__set_container_id(old_container_id);
                //  task->__set_status(task_status_update_error);
                //  task->error_times = 0;
                ////  m_task_db.write_task_to_db(task);
                //  LOG_INFO << "remove container failure. recover old_container:" << task->task_id;
                //  return E_DEFAULT;
             // }

              //  bool can_delete_image=CONTAINER_WORKER_IF->can_delete_image(image_id);//is or not delete image,can not delete original images

        std::shared_ptr<container_inspect_response> resp = CONTAINER_WORKER_IF->inspect_container(
            old_container_id);
        bool is_exsit_old_container = false;
        if (resp == nullptr) {
            sleep(3);
            resp = CONTAINER_WORKER_IF->inspect_container(old_container_id);
        }

        if (resp == nullptr) //用第二种方法判断 旧容器知否存在
        {
            sleep(6);
            std::string container_id = CONTAINER_WORKER_IF->get_container_id_current(task->task_id);
            if (!container_id.empty()) {
                is_exsit_old_container = true;
            }

        }

        if (resp != nullptr || is_exsit_old_container)//如果旧的镜像还存在，则删除
        {
            if (E_SUCCESS != CONTAINER_WORKER_IF->remove_container(old_container_id))//delete old container
            {
                CONTAINER_WORKER_IF->remove_container(task->container_id);//delete new docker
                CONTAINER_WORKER_IF->delete_image(training_engine_new);//delete new image
                CONTAINER_WORKER_IF->start_container(old_container_id);//start original container_id
                task->__set_container_id(old_container_id);
                task->__set_status(task_status_update_error);
                task->error_times = 0;
                //  m_task_db.write_task_to_db(task);
                LOG_INFO << "remove container failure. recover old_container:" << task->task_id;
                return E_DEFAULT;
            }//else// if(can_delete_image){//旧的镜像 因为有新生成的子镜像，故无法删除

            //if(E_SUCCESS!=CONTAINER_WORKER_IF->remove_image(image_id)){//delete old image
            //     CONTAINER_WORKER_IF->remove_image(image_id);
            // }
            // }
        }


        LOG_INFO << "delete old container success , task id:" << old_container_id;
        if (E_SUCCESS != CONTAINER_WORKER_IF->rename_container(task->task_id, autodbcimage_version)) {
            LOG_INFO << "rename container failure";

            CONTAINER_WORKER_IF->rename_container(task->task_id, autodbcimage_version);

        }

        LOG_INFO << "rename container success , task id:" << task->task_id;
    }
    LOG_INFO << "start_task_from_new_image success. Task id:";

    int32_t ret = CONTAINER_WORKER_IF->start_container(task->container_id);//start new container_id

    if (ret != E_SUCCESS) {
        task->__set_status(task_status_update_error);
        LOG_ERROR << "Start task error. Task id:" << task->task_id;
        return E_NO_START_CONTAINER;
    }

    LOG_INFO << "start_task_from_new_image success. Task id:" << task->task_id;
    // task->__set_start_time(time_util::get_time_stamp_ms());
    task->__set_status(task_status_running);
    task->error_times = 0;
    LOG_INFO << "update task status:" << "task_status_running";
    //  m_task_db.write_task_to_db(task);
    LOG_INFO << "update task status:" << "write_task_to_db";
    LOG_INFO << "update E_SUCCESS:" << E_SUCCESS;
    return E_SUCCESS;
}

int32_t task_scheduling::create_task_from_image(std::shared_ptr<ai_training_task> task,
    std::string autodbcimage_version) {
    if (nullptr == task) {
        return E_DEFAULT;
    }

    if (task->entry_file.empty() || task->code_dir.empty() || task->task_id.empty()) {
        LOG_INFO << "task config error.";
        return E_DEFAULT;
    }

    //   std::string container_id_original=task->container_id;
    std::shared_ptr<container_config> config = m_container_worker->get_container_config_from_image(task);
    std::string task_id = task->task_id;


    std::string container_name = task_id + "_DBC_" + autodbcimage_version;
    if (CONTAINER_WORKER_IF->exist_container(container_name) != E_CONTAINER_NOT_FOUND) {
        CONTAINER_WORKER_IF->remove_container(container_name);
    }
    std::shared_ptr<container_create_resp> resp = CONTAINER_WORKER_IF->create_container(config, task_id,
        autodbcimage_version);

    if (resp != nullptr && !resp->container_id.empty()) {
        task->__set_container_id(resp->container_id);
        LOG_INFO << "create from_image task success. task id:" << task->task_id << " container id:"
            << task->container_id;

        return E_SUCCESS;
    }
    else {
        // sleep(90);
        LOG_INFO << "exist_container ?";
        if (CONTAINER_WORKER_IF->exist_container_time(container_name, 240) != E_CONTAINER_NOT_FOUND) {
            LOG_INFO << "exist_container yes";
        }
        else {
            LOG_ERROR << "create task failed. task id:" << task->task_id;
        }

    }
    return E_DEFAULT;
}

int32_t task_scheduling::create_task(std::shared_ptr<ai_training_task> task, bool is_docker) {
    if (nullptr == task) {
        return E_DEFAULT;
    }

    if (task->entry_file.empty() || task->code_dir.empty() || task->task_id.empty()) {
        LOG_INFO << "task config error.";
        return E_DEFAULT;
    }

    if (is_docker) {
        if (CONTAINER_WORKER_IF->exist_container(task->task_id) != E_CONTAINER_NOT_FOUND) {
            CONTAINER_WORKER_IF->remove_container(task->task_id);
        }
        std::shared_ptr<container_config> config = m_container_worker->get_container_config(task);
        std::shared_ptr<container_create_resp> resp = CONTAINER_WORKER_IF->create_container(config,
            task->task_id, "");
        if (resp != nullptr && !resp->container_id.empty()) {
            task->__set_container_id(resp->container_id);
            LOG_INFO << "create task success. task id:" << task->task_id << " container id:"
                << task->container_id;

            return E_SUCCESS;
        }
        else {
            sleep(60);
            std::string container_name = task->task_id;
            LOG_INFO << "exist_container ?";
            std::string container_id = CONTAINER_WORKER_IF->get_container_id(container_name);

            if (container_id.compare("error") != 0) {
                LOG_INFO << "exist_container yes";
                task->__set_container_id(container_id);
                LOG_INFO << "create task success. task id:" << task->task_id << " container id:"
                    << task->container_id;

                return E_SUCCESS;
            }
            else {
                LOG_INFO << "exist_container no";
                LOG_ERROR << "create task failed. task id:" << task->task_id;
            }

        }
    }
    else {
        if (VM_WORKER_IF->existDomain(task->task_id)) {
            VM_WORKER_IF->destoryDomain(task->task_id);
        }

        // 解析ip port
        boost::property_tree::ptree pt;
        std::string host_ip, transform_port;
        try {
            std::stringstream ss;
            ss << task->server_specification;
            boost::property_tree::read_json(ss, pt);

            if (pt.count("port") != 0) {
                transform_port = pt.get<std::string>("port.22");
            }

            host_ip = run_shell("dig +short myip.opendns.com @resolver1.opendns.com");
            host_ip = string_util::rtrim(host_ip, '\n');
            LOG_DEBUG << "create_domain host_ip: " << host_ip << ", transform_port: " << transform_port;
        }
        catch (...) {

        }

        int32_t ret = VM_WORKER_IF->createDomain(task->task_id, host_ip, transform_port,
            "/data/" + task->training_engine);
        if (ret == E_SUCCESS) {
            LOG_INFO << "create vm task success. task id:" << task->task_id;
            return E_SUCCESS;
        }
        else {
            LOG_ERROR << "create vm task failed. task id:" << task->task_id << " ret:" << ret;
        }
    }

    return E_DEFAULT;
}

int32_t task_scheduling::start_task(std::shared_ptr<ai_training_task> task, bool is_docker) {
    if (nullptr == task) {
        return E_SUCCESS;
    }

    auto state = get_task_state(task, is_docker);
    if (DBC_TASK_RUNNING == state) {
        if (task->status != task_status_running) {
            task->__set_status(task_status_running);
            m_task_db.write_task_to_db(task);
        }

        int32_t ret = E_EXIT_FAILURE;

        if (is_docker)
            ret = CONTAINER_WORKER_IF->restart_container(task->container_id);
        else
            ret = VM_WORKER_IF->rebootDomain(task->task_id);

        if (ret != E_SUCCESS) {
            LOG_ERROR << "restart task error. Task id:" << task->task_id;
            LOG_ERROR << "restart task error. container_id:" << task->container_id;
            return E_DEFAULT;
        }
        LOG_DEBUG << "task have been restarted, task id:" << task->task_id;
        task->__set_start_time(time_util::get_time_stamp_ms());
        task->__set_status(task_status_running);
        task->error_times = 0;
        LOG_INFO << "task status:" << "task_status_running";
        m_task_db.write_task_to_db(task);
        LOG_INFO << "task status:" << "write_task_to_db";
        LOG_INFO << "E_SUCCESS:" << E_SUCCESS;
        return E_SUCCESS;
    }

    int32_t ret = E_EXIT_FAILURE;
    if (is_docker) {
        //if image do not exist, then pull it
        if (E_IMAGE_NOT_FOUND == CONTAINER_WORKER_IF->exist_docker_image(task->training_engine, 15)) {
            return start_pull_image(task);
        }

        bool is_container_existed = (!task->container_id.empty());
        if (!is_container_existed) {
            //if container_id does not exist, means dbc need to create container
            if (E_SUCCESS != create_task(task, is_docker)) {
                LOG_ERROR << "create task error";
                return E_DEFAULT;
            }
        }


        // update container's parameter if
        std::string path = env_manager::get_home_path().generic_string() + "/container/parameters";
        std::string text = "task_id=" + task->task_id + "\n";

        LOG_INFO << " container_id: " << task->container_id << " task_id: " << task->task_id;

        if (!file_util::write_file(path, text)) {
            LOG_ERROR << "fail to refresh task's code_dir before reusing existing container for new task "
                << task->task_id;
            return E_DEFAULT;
        }

        ret = CONTAINER_WORKER_IF->start_container(task->container_id);
    }
    else {
        //not exsit, create
        if (!VM_WORKER_IF->existDomain(task->task_id))
            ret = create_task(task, is_docker);
        else
            ret = VM_WORKER_IF->startDomain(task->task_id);
    }

    if (ret != E_SUCCESS) {
        LOG_ERROR << "Start task error. Task id:" << task->task_id;
        return E_DEFAULT;
    }

    LOG_INFO << "start task success. Task id:" << task->task_id;
    task->__set_start_time(time_util::get_time_stamp_ms());
    task->__set_status(task_status_running);
    task->error_times = 0;
    LOG_INFO << "task status:" << "task_status_running";
    m_task_db.write_task_to_db(task);
    LOG_INFO << "task status:" << "write_task_to_db";
    LOG_INFO << "E_SUCCESS:" << E_SUCCESS;
    return E_SUCCESS;
}

int32_t task_scheduling::restart_task(std::shared_ptr<ai_training_task> task, bool is_docker) {
    if (nullptr == task) {
        return E_SUCCESS;
    }
    if (task->status == task_status_update_error)//如果任务是升级错误状态，则只修改任务状态为running
    {
        LOG_INFO << "update task status success. Task id:" << task->task_id;
        task->__set_start_time(time_util::get_time_stamp_ms());
        task->__set_status(task_status_running);
        task->error_times = 0;
        LOG_INFO << "task status:" << "task_status_running";
        m_task_db.write_task_to_db(task);
        LOG_INFO << "task status:" << "write_task_to_db";
        LOG_INFO << "E_SUCCESS:" << E_SUCCESS;
        return E_SUCCESS;
    }

    auto state = get_task_state(task, is_docker);
    if (DBC_TASK_RUNNING == state) {
        if (task->status != task_status_running) {
            task->__set_status(task_status_running);
            m_task_db.write_task_to_db(task);
        }
        int32_t ret = E_EXIT_FAILURE;

        if (is_docker)
            ret = CONTAINER_WORKER_IF->restart_container(task->container_id);
        else
            ret = VM_WORKER_IF->rebootDomain(task->task_id);

        if (ret != E_SUCCESS) {
            LOG_ERROR << "restart task error. Task id:" << task->task_id;
            LOG_ERROR << "restart task error. container_id:" << task->container_id;
            return E_DEFAULT;
        }
        LOG_DEBUG << "task have been restarted, task id:" << task->task_id;

        return E_SUCCESS;
    }
    else if (DBC_TASK_STOPPED == state) {
        int32_t ret = E_EXIT_FAILURE;

        if (is_docker)
            ret = CONTAINER_WORKER_IF->start_container(task->container_id);
        else
            ret = VM_WORKER_IF->startDomain(task->task_id);

        if (ret != E_SUCCESS) {
            LOG_ERROR << "Start task error. Task id:" << task->task_id;
            return E_DEFAULT;
        }
    }
    else {
        LOG_ERROR << "Start task error. Task id:" << task->task_id;
        return E_DEFAULT;
    }


    LOG_INFO << "start task success. Task id:" << task->task_id;
    task->__set_start_time(time_util::get_time_stamp_ms());
    task->__set_status(task_status_running);
    task->error_times = 0;
    LOG_INFO << "task status:" << "task_status_running";
    m_task_db.write_task_to_db(task);
    LOG_INFO << "task status:" << "write_task_to_db";
    LOG_INFO << "E_SUCCESS:" << E_SUCCESS;
    return E_SUCCESS;
}

int32_t task_scheduling::stop_task(std::shared_ptr<ai_training_task> task) {
    if (nullptr == task) {
        return E_SUCCESS;
    }

    bool is_docker = false;
    if (task->server_specification.find("docker") != std::string::npos)
        is_docker = true;

    LOG_INFO << "stop task " << task->task_id;
    task->__set_end_time(time_util::get_time_stamp_ms());
    m_task_db.write_task_to_db(task);

    if (is_docker) {
        if (task->container_id.empty()) {
            return CONTAINER_WORKER_IF->stop_container(task->task_id);
        }
        else {
            return CONTAINER_WORKER_IF->stop_container(task->container_id);
        }
    }
    else {
        VM_WORKER_IF->shutdownDomain(task->task_id);
        vm_status status = VM_WORKER_IF->getDomainStatus(task->task_id);
        if (status == vm_running) {
            VM_WORKER_IF->destoryDomain(task->task_id);
        }
        return E_SUCCESS;
    }
}

int32_t task_scheduling::stop_task_only_id(std::string task_id, bool is_docker) {
    if (is_docker)
        return CONTAINER_WORKER_IF->stop_container(task_id);
    else {
        VM_WORKER_IF->shutdownDomain(task_id);
        vm_status status = VM_WORKER_IF->getDomainStatus(task_id);
        if (status == vm_running) {
            VM_WORKER_IF->destoryDomain(task_id);
        }
        return E_SUCCESS;
    }
}

int32_t task_scheduling::delete_task(std::shared_ptr<ai_training_task> task, bool is_docker) {
    if (nullptr == task) {
        return E_SUCCESS;
    }

    try {
        if (DBC_TASK_RUNNING == get_task_state(task, is_docker)) {
            stop_task(task);
        }
        if (is_docker) {
            if (!task->container_id.empty()) {
                CONTAINER_WORKER_IF->remove_container(task->container_id);
            }
        }
        else {
            VM_WORKER_IF->destoryDomain(task->task_id);
        }
        m_task_db.delete_task(task);
    }
    catch (...) {
        LOG_ERROR << "delete task abnormal";
        return E_DEFAULT;
    }

    return E_SUCCESS;
}

TASK_STATE task_scheduling::get_task_state(std::shared_ptr<ai_training_task> task, bool is_docker) {
    if (nullptr == task || task->task_id.empty()) {
        return DBC_TASK_NULL;
    }
    if (is_docker) {
        // inspect container
        std::string container_id = task->container_id;

        // container can be started again by task delivered latter,
        // in that case, the container's id and name keeps the original value, then new task's id and container's name does not equal any more.
        if (!task->container_id.empty()) {
            container_id = task->container_id;
        }

        std::shared_ptr<container_inspect_response> resp = CONTAINER_WORKER_IF->inspect_container(container_id);
        if (nullptr == resp) {
            // LOG_ERROR << "set_container_id:" << "null";
            //task->__set_container_id("");
            return DBC_TASK_NOEXIST;
        }

        //local db may be deleted, but task is running, then container_id is empty.
        if (!resp->id.empty() && (task->container_id.empty() || resp->id != task->container_id)) {
            task->__set_container_id(resp->id);
        }

        if (true == resp->state.running) {
            return DBC_TASK_RUNNING;
        }
    }
    else {
        string task_id = task->task_id;
        if (!VM_WORKER_IF->existDomain(task_id)) {
            return DBC_TASK_NOEXIST;
        }

        vm_status status = VM_WORKER_IF->getDomainStatus(task_id);
        if (status == vm_running) {
            return DBC_TASK_RUNNING;
        }

    }
    return DBC_TASK_STOPPED;
}

int32_t task_scheduling::start_pull_image(std::shared_ptr<ai_training_task> task) {
    if (nullptr == task || task->training_engine.empty()) {
        return E_SUCCESS;
    }

    //check local evn.
    auto ret = m_container_worker->can_pull_image();
    if (E_SUCCESS != ret) {
        return ret;
    }

    if (nullptr == m_pull_image_mng) {
        m_pull_image_mng = std::make_shared<image_manager>();
    }

    //if the task pulling image is not the task need image.
    if (!m_pull_image_mng->get_pull_image_name().empty()
        && m_pull_image_mng->get_pull_image_name() != task->training_engine) {
        if (PULLING == m_pull_image_mng->check_pull_state()) {
            m_pull_image_mng->terminate_pull();
        }
    }

    if (PULLING == m_pull_image_mng->check_pull_state()) {
        return E_SUCCESS;
    }

    if (E_SUCCESS != m_pull_image_mng->start_pull(task->training_engine)) {
        LOG_ERROR << "task engine pull fail. engine:" << task->training_engine;
        return E_PULLING_IMAGE;
    }

    m_pull_image_mng->set_start_time(time_util::get_time_stamp_ms());

    if (task_status_queueing == task->status) {
        task->__set_status(task_status_pulling_image);
        LOG_DEBUG << "docker pulling image. change status to " << to_training_task_status_string(task->status)
            << " task id:" << task->task_id << " image engine:" << task->training_engine;
        m_task_db.write_task_to_db(task);
    }

    return E_SUCCESS;
}

int32_t task_scheduling::stop_pull_image(std::shared_ptr<ai_training_task> task) {
    if (!m_pull_image_mng) {
        return E_SUCCESS;
    }

    if (task->training_engine != m_pull_image_mng->get_pull_image_name()) {
        LOG_ERROR << "pull image is not " << task->training_engine;
        return E_SUCCESS;
    }

    LOG_INFO << "terminate pull " << task->training_engine;
    m_pull_image_mng->terminate_pull();

    return E_SUCCESS;
}

void task_scheduling::appendAllTaskStatus(std::vector<matrix::service_core::task_status>& vec) {
    for (auto& it : m_training_tasks) {
        matrix::service_core::task_status ts;
        ts.task_id = it.second->task_id;
        ts.status = it.second->status;
        ts.pwd = it.second->pwd;
        vec.push_back(ts);
    }
}

*/

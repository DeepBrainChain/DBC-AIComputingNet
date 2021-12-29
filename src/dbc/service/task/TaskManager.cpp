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
#include "util/system_info.h"
#include "config/conf_manager.h"
#include "tinyxml2.h"
#include "ImageManager.h"

TaskManager::TaskManager() {

}

TaskManager::~TaskManager() {
    this->stop();
}

FResult TaskManager::init() {
    m_httpclient.connect_chain();

    if (!m_vm_client.Init()) {
        return {E_DEFAULT, "connect libvirt tcp service failed"};
    }

    if (!TaskInfoMgr::instance().init()) {
        return {E_DEFAULT, "task info manager init failed"};
    }

    if (!TaskIptableMgr::instance().init()) {
        return {E_DEFAULT, "task iptable manager init failed"};
    }

    if (!WalletSessionIDMgr::instance().init()) {
        return {E_DEFAULT, "wallet sessionid manager init failed"};
    }

    if (!WalletRentTaskMgr::instance().init()) {
        return {E_DEFAULT, "wallet renttask manager init failed;"};
    }

    auto tasks = TaskInfoMgr::instance().getTasks();
    for (auto& it : tasks) {
        dbclog::instance().add_task_log_backend(it.second->task_id);
    }

    if (!restore_tasks()) {
        return {E_DEFAULT, "restore tasks failed"};
    }

    shell_remove_reject_iptable_from_system();

    std::vector<std::string> taskids = TaskInfoMgr::instance().getAllTaskId();
    TaskResourceMgr::instance().init(taskids);
    SnapshotManager::instance().init(taskids);

    return {E_SUCCESS, ""};
}

FResult TaskManager::listImages(const std::string& wallet, std::vector<std::string> &images) {
    return ImageManager::instance().ListAllImages(images);
}

FResult TaskManager::downloadImage(const std::string& wallet, const std::string &image_name) {
    std::vector<std::string> images;
    FResult ret = ImageManager::instance().ListAllImages(images);
    if (std::get<0>(ret) != E_SUCCESS) {
        return ret;
    }
    auto iter = std::find(images.begin(), images.end(), image_name);
    if (iter == images.end()) {
        return {E_NOT_FOUND, "image not exist"};
    }
    DownloadImageEvent diEvent;
    diEvent.task_id = util::create_task_id();
    diEvent.images.push_back(image_name);
    ImageManager::instance().PushDownloadEvent(diEvent, nullptr);
    return FResultOK;
}

FResult TaskManager::uploadImage(const std::string& wallet, const std::string &image_name) {
    std::vector<std::string> images;
    FResult ret = ImageManager::instance().ListAllImages(images);
    if (std::get<0>(ret) != E_SUCCESS) {
        return ret;
    }
    auto iter = std::find(images.begin(), images.end(), image_name);
    if (iter != images.end()) {
        return {E_NOT_FOUND, "image already exist, please rename the image file that needs to be uploaded"};
    }
    UploadImageEvent uiEvent;
    uiEvent.task_id = util::create_task_id();
    uiEvent.image = image_name;
    ImageManager::instance().PushUploadEvent(uiEvent);
    return FResultOK;
}

bool TaskManager::restore_tasks() {
    std::string cur_renter_wallet;
    int64_t cur_rent_end = 0;

    std::vector<std::string> wallets = WalletRentTaskMgr::instance().getAllWallet();
    for (auto& it : wallets) {
        int64_t rent_end = m_httpclient.request_rent_end(it);
        if (rent_end > 0) {
            cur_renter_wallet = it;
            cur_rent_end = rent_end;
            break;
        }
    }

    // 将不是当前租用者的task都停掉
    //【如果存在当前租用者，就把验证人创建的虚拟机也一起关闭】
    //【如果不存在当前租用者，就先不要关闭验证人的虚拟机】
    auto renttasks = WalletRentTaskMgr::instance().getRentTasks();
    for (auto& it : renttasks) {
        if (it.first != cur_renter_wallet) {
            for (auto& id : it.second->task_ids) {
                if (cur_renter_wallet.empty()) {
                    if (!util::check_id(id)) continue;
                }

                close_task(id);
            }
        }
    }

    // 如果不存在当前租用者，恢复验证人task
    if (cur_renter_wallet.empty()) {
        std::map<std::string, std::shared_ptr<dbc::TaskInfo> > check_tasks;
        for (auto& it : renttasks) {
            for (auto& task_id : it.second->task_ids) {
                auto taskinfo = TaskInfoMgr::instance().getTaskInfo(task_id);
                if (taskinfo != nullptr && taskinfo->task_id.find("vm_check_") != std::string::npos) {
                    check_tasks.insert({taskinfo->task_id, taskinfo});
                }
            }

            if (!check_tasks.empty())
                break;
        }

        for (auto& it_task : check_tasks) {
            int32_t task_status = it_task.second->status;
            if (task_status == ETaskStatus::TS_Running) {
                start_task(it_task.first);
            }
        }
    }
    // 如果存在当前正在租用的用户，恢复租用者task
    else {
        do {
            // 将没有归属的task（除了验证人task）都归到当前租用的用户下
            auto wallet_renttasks = WalletRentTaskMgr::instance().getRentTasks();
            auto it = wallet_renttasks.find(cur_renter_wallet);
            if (it == wallet_renttasks.end()) {
                auto tmp_tasks = TaskInfoMgr::instance().getTasks();
                for (auto &it: wallet_renttasks) {
                    for (auto &id: it.second->task_ids) {
                        tmp_tasks.erase(id);
                    }
                }

                std::shared_ptr<dbc::rent_task> renttask = std::make_shared<dbc::rent_task>();
                renttask->rent_wallet = cur_renter_wallet;
                renttask->rent_end = cur_rent_end;
                for (auto &id: tmp_tasks) {
                    if (util::check_id(id.first)) { // 排除验证人task
                        renttask->task_ids.push_back(id.first);
                    }
                }
                WalletRentTaskMgr::instance().update(renttask);
            }
        } while(0);

        auto wallet_renttasks = WalletRentTaskMgr::instance().getRentTasks();
        auto iter = wallet_renttasks.find(cur_renter_wallet);
        for (auto& id : iter->second->task_ids) {
            auto taskinfo = TaskInfoMgr::instance().getTaskInfo(id);
            if (taskinfo != nullptr) {
                if (taskinfo->status == ETaskStatus::TS_Running) {
                    start_task(taskinfo->task_id);
                }
            }
        }
    }

    return true;
}

void TaskManager::start_task(const std::string &task_id) {
    if (m_vm_client.StartDomain(task_id)) {
        auto taskinfo = TaskInfoMgr::instance().getTaskInfo(task_id);
        if (taskinfo != nullptr) {
            taskinfo->status = ETaskStatus::TS_Running;
            TaskInfoMgr::instance().update(taskinfo);

            add_iptable_to_system(task_id);
        }
    }
}

void TaskManager::close_task(const std::string &task_id) {
    if (m_vm_client.DestroyDomain(task_id) == E_SUCCESS) {
        auto taskinfo = TaskInfoMgr::instance().getTaskInfo(task_id);
        if (taskinfo != nullptr) {
            taskinfo->status = ETaskStatus::TS_ShutOff;
            TaskInfoMgr::instance().update(taskinfo);
            remove_iptable_from_system(task_id);
        }
    }
}

void TaskManager::remove_iptable_from_system(const std::string& task_id) {
    auto iptable = TaskIptableMgr::instance().getIptable(task_id);
    if (iptable != nullptr) {
        std::string host_ip = iptable->host_ip;
        std::string ssh_port = iptable->ssh_port;
        std::string rdp_port = iptable->rdp_port;
        std::vector<std::string> custom_port = iptable->custom_port;
        std::string vm_local_ip = iptable->vm_local_ip;

        shell_remove_iptable_from_system(task_id, host_ip, ssh_port, vm_local_ip);
    }
}

void TaskManager::add_iptable_to_system(const std::string &task_id) {
    auto iptable = TaskIptableMgr::instance().getIptable(task_id);
    if (iptable != nullptr) {
        std::string host_ip = iptable->host_ip;
        std::string ssh_port = iptable->ssh_port;
        std::string rdp_port = iptable->rdp_port;
        std::vector<std::string> custom_port = iptable->custom_port;
        std::string vm_local_ip = iptable->vm_local_ip;

        shell_add_iptable_to_system(task_id, host_ip, ssh_port, rdp_port, custom_port, vm_local_ip);
    }
}

void TaskManager::shell_remove_iptable_from_system(const std::string& task_id, const std::string &public_ip,
                                                   const std::string &ssh_port, const std::string &vm_local_ip) {
    // remove chain
    std::string chain_name = "chain_" + task_id;
    std::string cmd = "sudo iptables -t nat -F " + chain_name;
    run_shell(cmd);
    cmd = "sudo iptables -t nat -D PREROUTING -j " + chain_name;
    run_shell(cmd);
    cmd = "sudo iptables -t nat -X " + chain_name;
    run_shell(cmd);

    // remove old rules
    cmd = "sudo iptables --table nat -D PREROUTING --protocol tcp --destination " + public_ip +
                      " --destination-port " + ssh_port + " --jump DNAT --to-destination " + vm_local_ip + ":22";
    run_shell(cmd.c_str());

    cmd = "sudo iptables -t nat -D PREROUTING -p tcp --dport " + ssh_port +
          " -j DNAT --to-destination " + vm_local_ip + ":22";
    run_shell(cmd.c_str());

    auto pos = vm_local_ip.rfind('.');
    std::string ip = vm_local_ip.substr(0, pos) + ".1";
    cmd = "sudo iptables -t nat -D POSTROUTING -p tcp --dport " + ssh_port + " -d " + vm_local_ip +
          " -j SNAT --to " + ip;
    run_shell(cmd.c_str());

    cmd = "sudo iptables -t nat -D PREROUTING -p tcp -m tcp --dport 20000:60000 -j DNAT --to-destination " +
          vm_local_ip + ":20000-60000";
    run_shell(cmd.c_str());

    cmd = "sudo iptables -t nat -D PREROUTING -p udp -m udp --dport 20000:60000 -j DNAT --to-destination " +
          vm_local_ip + ":20000-60000";
    run_shell(cmd.c_str());

    cmd = "sudo iptables -t nat -D POSTROUTING -d " + vm_local_ip +
          " -p tcp -m tcp --dport 20000:60000 -j SNAT --to-source " + public_ip;
    run_shell(cmd.c_str());

    cmd = "sudo iptables -t nat -D POSTROUTING -d " + vm_local_ip +
          " -p udp -m udp --dport 20000:60000 -j SNAT --to-source " + public_ip;
    run_shell(cmd.c_str());

    cmd = "sudo iptables -t nat -D PREROUTING -p tcp -d " + public_ip + " --dport " + ssh_port
                      + " -j DNAT --to-destination " + vm_local_ip + ":22";
    run_shell(cmd.c_str());

    pos = vm_local_ip.rfind('.');
    ip = vm_local_ip.substr(0, pos) + ".0/24";
    cmd = "sudo iptables -t nat -D POSTROUTING -s " + ip + " -j SNAT --to-source " + public_ip;
    run_shell(cmd.c_str());

    cmd = "sudo iptables -t nat -D PREROUTING -p tcp -d " + public_ip + " --dport 6000:60000 -j DNAT --to-destination "
          + vm_local_ip + ":6000-60000";
    run_shell(cmd.c_str());
}

void TaskManager::shell_add_iptable_to_system(const std::string& task_id, const std::string &public_ip,
                                              const std::string &ssh_port, const std::string &rdp_port,
                                              const std::vector<std::string>& custom_port,
                                              const std::string &vm_local_ip) {
    // remove chain
    std::string chain_name = "chain_" + task_id;
    std::string cmd = "sudo iptables -t nat -F " + chain_name;
    run_shell(cmd);
    cmd = "sudo iptables -t nat -D PREROUTING -j " + chain_name;
    run_shell(cmd);
    cmd = "sudo iptables -t nat -X " + chain_name;
    run_shell(cmd);

    // add chain and rules
    cmd = "sudo iptables -t nat -N " + chain_name;
    run_shell(cmd);
    if (!ssh_port.empty() && atoi(ssh_port) > 0) {
        cmd = "sudo iptables -t nat -A " + chain_name + " -p tcp --dport " + ssh_port +
              " --jump DNAT --to-destination " + vm_local_ip + ":22";
        run_shell(cmd);
    }
    if (!rdp_port.empty() && atoi(rdp_port) > 0) {
        cmd = "sudo iptables -t nat -A " + chain_name + " -p tcp --dport " + rdp_port +
              " --jump DNAT --to-destination " + vm_local_ip + ":3389";
        run_shell(cmd);
    }

    for (auto& str : custom_port) {
        std::vector<std::string> v_protocol_port = util::split(str, ",");
        if (v_protocol_port.size() != 2 || (v_protocol_port[0] != "tcp" && v_protocol_port[0] != "udp")) {
            continue;
        }

        std::string s_protocol = v_protocol_port[0];
        util::trim(s_protocol);
        std::string s_port = v_protocol_port[1];
        util::trim(s_port);

        if (util::is_digits(s_port)) {
            cmd = "sudo iptables -t nat -A " + chain_name + " -p " + s_protocol + " --dport " + s_port +
                  " --jump DNAT --to-destination " + vm_local_ip + ":" + s_port;
            run_shell(cmd);
            continue;
        }

        if (s_port.find(':') != std::string::npos) {
            std::vector<std::string> vec = util::split(s_port, ":");
            if (vec.size() == 2 && util::is_digits(vec[0]) && util::is_digits(vec[1])) {
                cmd = "sudo iptables -t nat -A " + chain_name + " -p " + s_protocol + " --dport " + vec[0] +
                      " --jump DNAT --to-destination " + vm_local_ip + ":" + vec[1];
                run_shell(cmd);
                continue;
            }
        }

        if (s_port.find('-') != std::string::npos) {
            std::vector<std::string> vec = util::split(s_port, "-");
            if (vec.size() == 2 && util::is_digits(vec[0]) && util::is_digits(vec[1])) {
                cmd = "sudo iptables -t nat -A " + chain_name + " -p " + s_protocol + " --dport " +
                        vec[0] + ":" + vec[1] + " -j DNAT --to-destination " + vm_local_ip + ":" +
                        vec[0] + "-" + vec[1];
                run_shell(cmd);
                continue;
            }
        }

        if (s_port.find(':') != std::string::npos && s_port.find('-') != std::string::npos) {
            std::vector<std::string> vec = util::split(s_port, ":");
            if (vec.size() == 2) {
                std::vector<std::string> vec1 = util::split(vec[0], "-");
                std::vector<std::string> vec2 = util::split(vec[1], "-");
                if (vec1.size() == 2 && vec2.size() == 2) {
                    cmd = "sudo iptables -t nat -A " + chain_name + " -p " + s_protocol + " --dport " +
                            vec1[0] + ":" + vec1[1] + " -j DNAT --to-destination " + vm_local_ip + ":" +
                            vec2[0] + "-" + vec2[1];
                    run_shell(cmd);
                    continue;
                }
            }
        }
    }

    // add ref
    cmd = "sudo iptables -t nat -I PREROUTING -j " + chain_name;
    run_shell(cmd);
}

void TaskManager::shell_remove_reject_iptable_from_system() {
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

void TaskManager::delete_task(const std::string &task_id) {
    unsigned int undefineFlags = 0;
    int32_t snapshotCount = SnapshotManager::instance().getTaskSnapshotCount(task_id);
    int32_t hasNvram = m_vm_client.IsDomainHasNvram(task_id);
    if (snapshotCount > 0) undefineFlags |= VIR_DOMAIN_UNDEFINE_SNAPSHOTS_METADATA;
    if (hasNvram > 0) undefineFlags |= VIR_DOMAIN_UNDEFINE_NVRAM;

    // domain
    m_vm_client.DestroyAndUndefineDomain(task_id, undefineFlags);

    // data
    auto taskinfo = TaskInfoMgr::instance().getTaskInfo(task_id);
    TaskInfoMgr::instance().delTaskInfo(task_id);

    auto iptable = TaskIptableMgr::instance().getIptable(task_id);
    if (iptable != nullptr) {
        std::string host_ip = iptable->host_ip;
        std::string ssh_port = iptable->ssh_port;
        std::string rdp_port = iptable->rdp_port;
        std::vector<std::string> custom_port = iptable->custom_port;
        std::string vm_local_ip = iptable->vm_local_ip;
        shell_remove_iptable_from_system(task_id, host_ip, ssh_port, vm_local_ip);
    }
    TaskIptableMgr::instance().delIptable(task_id);

    WalletRentTaskMgr::instance().delRentTask(task_id);

    TaskResourceMgr::instance().delTaskResource(task_id);

    // need to delete snapshot source file ?
    {
        std::vector<std::shared_ptr<dbc::snapshotInfo>> snaps;
        SnapshotManager::instance().listTaskSnapshot(task_id, snaps, false);
        for (const auto& snap : snaps) {
            if (snap->disks.empty()) continue;
            for (const auto& disk : snap->disks) {
                if (!disk.source_file.empty() && fs::is_regular_file(disk.source_file)) {
                    remove(disk.source_file.c_str());
                    TASK_LOG_INFO(task_id, "remove snapshot file: " << disk.source_file);
                }
            }
        }
    }
    SnapshotManager::instance().delTaskSnapshot(task_id);

    // disk
    if (taskinfo != nullptr)
        delete_disk_system_file(task_id, taskinfo->image_name);

    delete_disk_data_file(task_id);
}

void TaskManager::delete_disk_system_file(const std::string &task_id, const std::string& disk_system_file_name) {
    std::string filename = util::GetFileNameFromPath(disk_system_file_name);
    std::string name = util::GetFileNameWithoutExt(filename);
    std::string ext = util::GetFileExt(filename);
    std::string image_full_path = "/data/" + name + "_" + task_id + "." + ext;
    if (fs::is_regular_file(image_full_path)) {
        remove(image_full_path.c_str());
        TASK_LOG_INFO(task_id, "remove disk(system) file: " << image_full_path);
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
            remove(it.c_str());
            TASK_LOG_INFO(task_id, "remove disk(data) file: " << it);
        }
    }
}

void TaskManager::start() {
    m_running = true;
    if (m_process_thread == nullptr) {
        m_process_thread = new std::thread(&TaskManager::process_task_thread_func, this);
    }

    if (m_prune_thread == nullptr) {
        m_prune_thread = new std::thread(&TaskManager::prune_task_thread_func, this);
    }
}

void TaskManager::stop() {
    m_running = false;
    if (m_process_thread != nullptr && m_process_thread->joinable()) {
        m_process_thread->join();
    }
    delete m_process_thread;

    if (m_prune_thread != nullptr && m_prune_thread->joinable()) {
        m_prune_thread->join();
    }
    delete m_prune_thread;
}

static std::string genpwd() {
    char chr[] = { 'A', 'B', 'C', 'D', 'E', 'F', 'G',
                   'H', 'I', 'J', 'K', 'L', 'M', 'N',
                   'O', 'P', 'Q', 'R', 'S', 'T',
                   'U', 'V', 'W', 'X', 'Y', 'Z',
                   'a', 'b', 'c', 'd', 'e', 'f', 'g',
                   'h', 'i', 'j', 'k', 'l', 'm', 'n',
                   'o', 'p', 'q', 'r', 's', 't',
                   'u', 'v', 'w', 'x', 'y', 'z',
                   '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
                   '_', '/', '+', '[', ']', '@', '#', '^'
    };

    struct timeval tv{};
    gettimeofday(&tv, nullptr);
    srand(tv.tv_usec);
    std::string strpwd;
    int idx0 = rand() % 52;
    char buf[2] = { 0 };
    sprintf(buf, "%c", chr[idx0]);
    strpwd.append(buf);

    for (int i = 0; i < 15; i++) {
        int idx0 = rand() % 70;
        sprintf(buf, "%c", chr[idx0]);
        strpwd.append(buf);
    }

    return strpwd;
}

FResult TaskManager::createTask(const std::string& wallet, const std::string &additional,
                                int64_t rent_end, USER_ROLE role, std::string& task_id) {
    TaskCreateParams createparams;
    FResult fret = parse_create_params(additional, role, createparams);
    if (std::get<0>(fret) != E_SUCCESS) {
        return fret;
    }

    task_id = createparams.task_id;

    std::shared_ptr<dbc::TaskInfo> taskinfo = std::make_shared<dbc::TaskInfo>();
    taskinfo->__set_task_id(createparams.task_id);
    taskinfo->__set_image_name(createparams.image_name);
    taskinfo->__set_data_file_name(createparams.data_file_name);
    taskinfo->__set_login_password(createparams.login_password);
    taskinfo->__set_ssh_port(std::to_string(createparams.ssh_port));
    taskinfo->__set_rdp_port(std::to_string(createparams.rdp_port));
    taskinfo->__set_custom_port(createparams.custom_port);
    int64_t now = time(nullptr);
    taskinfo->__set_create_time(now);
    taskinfo->__set_last_start_time(now);
    taskinfo->__set_vm_xml(createparams.vm_xml);
    taskinfo->__set_vm_xml_url(createparams.vm_xml_url);
    taskinfo->__set_status(TS_Creating);
    taskinfo->__set_operation_system(createparams.operation_system);
    taskinfo->__set_bios_mode(createparams.bios_mode);
    TaskInfoMgr::instance().addTaskInfo(taskinfo);

    std::shared_ptr<TaskResource> task_resource = std::make_shared<TaskResource>();
    task_resource->cpu_sockets = createparams.cpu_sockets;
    task_resource->cpu_cores = createparams.cpu_cores;
    task_resource->cpu_threads = createparams.cpu_threads;
    task_resource->gpus = createparams.gpus;
    task_resource->mem_size = createparams.mem_size;
    task_resource->disk_system_size = g_disk_system_size * 1024L * 1024L;
    task_resource->disks = createparams.disks;
    task_resource->vnc_port = createparams.vnc_port;
    task_resource->vnc_password = createparams.vnc_password;
    TaskResourceMgr::instance().addTaskResource(taskinfo->task_id, task_resource);

    WalletRentTaskMgr::instance().addRentTask(wallet, taskinfo->task_id, rent_end);

    dbclog::instance().add_task_log_backend(taskinfo->task_id);

    ETaskEvent ev;
    ev.task_id = taskinfo->task_id;
    ev.op = T_OP_Create;
    add_process_task(ev);
    return FResultOK;
}

FResult TaskManager::parse_create_params(const std::string &additional, USER_ROLE role, TaskCreateParams& params) {
    if (additional.empty()) {
        return {E_DEFAULT, "additional is empty"};
    }

    rapidjson::Document doc;
    doc.Parse(additional.c_str());
    if (!doc.IsObject()) {
        return {E_DEFAULT, "additional(json) parse failed"};
    }

    std::string image_name, s_ssh_port, s_rdp_port, s_gpu_count, s_cpu_cores, s_mem_size,
            s_disk_size, vm_xml, vm_xml_url, s_vnc_port, data_file_name, operation_system, bios_mode;
    std::vector<std::string> custom_ports;

    JSON_PARSE_STRING(doc, "image_name", image_name)
    JSON_PARSE_STRING(doc, "ssh_port", s_ssh_port)
    JSON_PARSE_STRING(doc, "rdp_port", s_rdp_port)
    if (doc.HasMember("custom_port")) {
        const rapidjson::Value& v_custom_port = doc["custom_port"];
        if (v_custom_port.IsArray()) {
            for (rapidjson::SizeType i = 0; i < v_custom_port.Size(); i++) {
                const rapidjson::Value& v_item = v_custom_port[i];
                if (v_item.IsString()) {
                    std::string str = v_item.GetString();
                    custom_ports.push_back(str);
                }
            }
        }
    }
    JSON_PARSE_STRING(doc, "gpu_count", s_gpu_count)
    JSON_PARSE_STRING(doc, "cpu_cores", s_cpu_cores)
    JSON_PARSE_STRING(doc, "mem_size", s_mem_size)     //G
    JSON_PARSE_STRING(doc, "disk_size", s_disk_size)   //G
    JSON_PARSE_STRING(doc, "vm_xml", vm_xml)
    JSON_PARSE_STRING(doc, "vm_xml_url", vm_xml_url)
    JSON_PARSE_STRING(doc, "vnc_port", s_vnc_port)
    JSON_PARSE_STRING(doc, "data_file_name", data_file_name)
    JSON_PARSE_STRING(doc, "operation_system", operation_system)
    JSON_PARSE_STRING(doc, "bios_mode", bios_mode)

    if (operation_system.empty()) {
        operation_system = "generic";
    }

    // rdp_port
    if (operation_system.find("win") != std::string::npos) {
        if (s_rdp_port.empty() || !util::is_digits(s_rdp_port) || atoi(s_rdp_port.c_str()) <= 0) {
            return {E_DEFAULT, "rdp_port is invalid"};
        }
    }
    // ssh_port
    else {
        if (s_ssh_port.empty() || !util::is_digits(s_ssh_port) || atoi(s_ssh_port.c_str()) <= 0) {
            return {E_DEFAULT, "ssh_port is invalid"};
        }
    }

    // vnc port
    if (s_vnc_port.empty()) {
        return {E_DEFAULT, "vnc port is not specified"};
    }

    int vnc_port = atoi(s_vnc_port.c_str());
    if (!util::is_digits(s_vnc_port) || vnc_port < 5900 || vnc_port > 6000) {
        return {E_DEFAULT, "vnc port out of range, it should be between 5900 and 6000"};
    }

    if (bios_mode.empty()) {
        bios_mode = "legacy";
    }

    std::string login_password = genpwd();
    std::string task_id;
    int32_t cpu_cores = 0, sockets = 0, cores = 0, threads = 0;
    int32_t gpu_count = 0;
    std::map<std::string, std::list<std::string>> gpus;
    uint64_t mem_size = 0; // KB
    std::map<int32_t, uint64_t> disks; // KB (下标从1开始)
    std::string vnc_password = login_password;

    if (vm_xml.empty() && vm_xml_url.empty()) {
        // image
        FResult fret = check_image(image_name);
        if (std::get<0>(fret) != E_SUCCESS) {
            return fret;
        }
        
        // data disk file name
        if (!data_file_name.empty()) {
            boost::filesystem::path data_path("/data/" + data_file_name);
            boost::system::error_code error_code;
            if (!boost::filesystem::exists(data_path, error_code) || error_code) {
                return {E_DEFAULT, "data file does not exist"};
            }

            if (!boost::filesystem::is_regular_file(data_path, error_code) || error_code) {
                return {E_DEFAULT, "data file is not a regular file"};
            }
        }

        // task_id
        task_id = util::create_task_id();
        if (role == USER_ROLE::UR_VERIFIER) {
            task_id = "vm_check_" + std::to_string(time(nullptr));
        }

        // cpu
        if (!util::is_digits(s_cpu_cores) || atoi(s_cpu_cores.c_str()) <= 0) {
            return {E_DEFAULT, "cpu_cores is invalid"};
        }

        cpu_cores = atoi(s_cpu_cores.c_str());

        if (role == USER_ROLE::UR_VERIFIER)
            cpu_cores = SystemInfo::instance().cpuinfo().total_cores;

        // gpu
        if (!util::is_digits(s_gpu_count) || atoi(s_gpu_count.c_str()) < 0) {
            return {E_DEFAULT, "gpu_count is invalid"};
        }

        gpu_count = atoi(s_gpu_count.c_str());

        if (role == USER_ROLE::UR_VERIFIER)
            gpu_count = SystemInfo::instance().gpuinfo().size();

        // mem
        if (!util::is_digits(s_mem_size) || atoi(s_mem_size.c_str()) <= 0) {
            return {E_DEFAULT, "mem_size is invalid"};
        }

        mem_size = atoi(s_mem_size.c_str()) * 1024L * 1024L;

        if (role == USER_ROLE::UR_VERIFIER)
            mem_size = SystemInfo::instance().meminfo().mem_free;

        // disk
        if (!util::is_digits(s_disk_size) || atoi(s_disk_size.c_str()) <= 0) {
            return {E_DEFAULT, "disk_size is invalid"};
        }

        uint64_t disk_size = atoi(s_disk_size.c_str()) * 1024L * 1024L;

        if (role == USER_ROLE::UR_VERIFIER)
            disk_size = (SystemInfo::instance().diskinfo().disk_available - g_disk_system_size * 1024L * 1024L) * 0.75;

        disks[1] = disk_size;

        // check
        if (!allocate_cpu(cpu_cores, sockets, cores, threads)) {
            LOG_ERROR << "allocate cpu failed";
            return {E_DEFAULT, "allocate cpu failed"};
        }

        if (!allocate_gpu(gpu_count, gpus)) {
            LOG_ERROR << "allocate gpu failed";
            return {E_DEFAULT, "allocate gpu failed"};
        }

        if (!allocate_mem(mem_size)) {
            LOG_ERROR << "allocate mem failed";
            return {E_DEFAULT, "allocate mem failed"};
        }

        if (!allocate_disk(disk_size)) {
            LOG_ERROR << "allocate disk failed";
            return {E_DEFAULT, "allocate disk failed"};
        }

        // check operation system
        fret = check_operation_system(operation_system);
        if (std::get<0>(fret) != E_SUCCESS) {
            return fret;
        }
        // check bios mode
        fret = check_bios_mode(bios_mode);
        if (std::get<0>(fret) != E_SUCCESS) {
            return fret;
        }
    }
    else {
        if (role != USER_ROLE::UR_RENTER_WALLET && role != USER_ROLE::UR_RENTER_SESSION_ID) {
            return {E_DEFAULT, "only renter can create task with vm_xml"};
        }

        std::string xml_file_path;

        if (!vm_xml.empty()) {
            xml_file_path = "/data/" + vm_xml;
            boost::filesystem::path xml_path(xml_file_path);
            if (!boost::filesystem::exists(xml_path)) {
                return {E_DEFAULT, xml_file_path + " not exist"};
            }
        }
        else if (!vm_xml_url.empty()) {
            std::string xml_file_content;
            if (vm_xml_url.substr(0, 7) == "http://") {
                auto pos = vm_xml_url.find('/', 7);
                std::string s_host = vm_xml_url.substr(7, pos - 7);
                int32_t host_port = 80;
                auto pos_1 = s_host.find(":");
                if (pos_1 != std::string::npos) {
                    host_port = atoi(s_host.substr(pos_1 + 1));
                    s_host = s_host.substr(0, pos_1);
                }
                std::string s_path = vm_xml_url.substr(pos);

                httplib::Client cli(s_host, host_port);
                std::shared_ptr<httplib::Response> resp = cli.Get(s_path.c_str(),
                                                                  [&](const char *data, size_t data_length) {
                                                                      xml_file_content.append(data, data_length);
                                                                      return true;
                                                                  });
            } else if (vm_xml_url.substr(0, 8) == "https://") {
                auto pos = vm_xml_url.find('/', 8);
                std::string s_host = vm_xml_url.substr(8, pos - 8);
                int32_t host_port = 443;
                auto pos_1 = s_host.find(":");
                if (pos_1 != std::string::npos) {
                    host_port = atoi(s_host.substr(pos_1 + 1));
                    s_host = s_host.substr(0, pos_1);
                }
                std::string s_path = vm_xml_url.substr(pos);

                httplib::SSLClient cli(s_host, host_port);
                std::shared_ptr<httplib::Response> resp = cli.Get(s_path.c_str(),
                                                                  [&](const char *data, size_t data_length) {
                                                                      xml_file_content.append(data, data_length);
                                                                      return true;
                                                                  });
            } else {
                return {E_DEFAULT, vm_xml_url + " is invalid (please use http:// or https://)"};
            }

            xml_file_path = "/data/" + util::GetFileNameFromPath(vm_xml_url);
            util::write_file(xml_file_path, xml_file_content);
            vm_xml = util::GetFileNameFromPath(xml_file_path);
        }

        ParseVmXmlParams xml_params;
        FResult fret = parse_vm_xml(xml_file_path, xml_params);
        if (std::get<0>(fret) != E_SUCCESS) {
            return fret;
        }

        task_id = std::move(xml_params.task_id);
        image_name = std::move(xml_params.image_name);
        data_file_name = std::move(xml_params.data_file_name);
        sockets = xml_params.cpu_sockets;
        cores = xml_params.cpu_cores;
        threads = xml_params.cpu_threads;
        cpu_cores = sockets * cores * threads;
        gpus = xml_params.gpus;
        gpu_count = gpus.size();
        mem_size = xml_params.mem_size;
        disks = xml_params.disks;
        vnc_port = xml_params.vnc_port;
        vnc_password = xml_params.vnc_password;
        operation_system = xml_params.operation_system;
        bios_mode = xml_params.bios_mode;

        // check
        // image
        fret = check_image(image_name);
        if (std::get<0>(fret) != E_SUCCESS) {
            return fret;
        }
        fret = check_data_image(data_file_name);
        if (std::get<0>(fret) != E_SUCCESS) {
            return fret;
        }
        // cpu
        fret = check_cpu(sockets, cores, threads);
        if (std::get<0>(fret) != E_SUCCESS) {
            return fret;
        }
        // gpu
        fret = check_gpu(gpus);
        if (std::get<0>(fret) != E_SUCCESS) {
            return fret;
        }
        // disk
        fret = check_disk(disks);
        if (std::get<0>(fret) != E_SUCCESS) {
            return fret;
        }
        // check operation system
        fret = check_operation_system(operation_system);
        if (std::get<0>(fret) != E_SUCCESS) {
            return fret;
        }
        // check bios mode
        fret = check_bios_mode(bios_mode);
        if (std::get<0>(fret) != E_SUCCESS) {
            return fret;
        }
    }

    // params
    params.task_id = task_id;
    params.login_password = login_password;
    params.image_name = image_name;
    params.data_file_name = data_file_name;
    params.ssh_port = atoi(s_ssh_port.c_str());
    params.rdp_port = atoi(s_rdp_port.c_str());
    params.custom_port = custom_ports;
    params.cpu_sockets = sockets;
    params.cpu_cores = cores;
    params.cpu_threads = threads;
    params.gpus = gpus;
    params.mem_size = mem_size;
    params.disks = disks;
    params.vm_xml = vm_xml;
    params.vm_xml_url = vm_xml_url;
    params.vnc_port = vnc_port;
    params.vnc_password = vnc_password;
    params.operation_system = operation_system;
    params.bios_mode = bios_mode;

    return {E_SUCCESS, "ok"};
}

bool TaskManager::allocate_cpu(int32_t& total_cores, int32_t& sockets, int32_t& cores_per_socket, int32_t& threads) {
    int32_t nTotalCores = SystemInfo::instance().cpuinfo().total_cores;
    if (total_cores > nTotalCores) return false;

    int32_t nSockets = SystemInfo::instance().cpuinfo().physical_cores;
    int32_t nCores = SystemInfo::instance().cpuinfo().physical_cores_per_cpu;
    int32_t nThreads = SystemInfo::instance().cpuinfo().threads_per_cpu;
    int32_t ret_total_cores = total_cores;
    threads = nThreads;

    for (int n = 1; n <= nSockets; n++) {
        int32_t tmp_cores = total_cores;
        while (tmp_cores % (n * threads) != 0)
            tmp_cores += 1;

        if (tmp_cores / (n * threads) <= nCores) {
            sockets = n;
            cores_per_socket = tmp_cores / (n * threads);
            ret_total_cores = tmp_cores;
            break;
        }
    }

    if (ret_total_cores == sockets * cores_per_socket * threads) {
        total_cores = ret_total_cores;
        return true;
    } else {
        return false;
    }
}

bool TaskManager::allocate_gpu(int32_t gpu_count, std::map<std::string, std::list<std::string>>& gpus) {
    if (gpu_count <= 0) return true;

    std::map<std::string, gpu_info> can_use_gpu = SystemInfo::instance().gpuinfo();
    auto tasks = TaskInfoMgr::instance().getTasks();
    for (auto& it : tasks) {
        if (it.second->status != TS_ShutOff && it.second->status < TS_Error) {
            auto task_resource = TaskResourceMgr::instance().getTaskResource(it.first);
            if (task_resource != nullptr) {
                const std::map<std::string, std::list<std::string>> &gpus = task_resource->gpus;
                for (auto &it1: gpus) {
                    can_use_gpu.erase(it1.first);
                }
            }
        }
    }

    int cur_count = 0;
    for (auto& it : can_use_gpu) {
        gpus[it.first] = it.second.devices;
        cur_count++;
        if (cur_count == gpu_count) break;
    }

    return cur_count == gpu_count;
}

bool TaskManager::allocate_mem(uint64_t mem_size) {
    return mem_size > 0 && mem_size <= SystemInfo::instance().meminfo().mem_free;
}

bool TaskManager::allocate_disk(uint64_t disk_size) {
    return disk_size > 0 && disk_size <= (SystemInfo::instance().diskinfo().disk_available - g_disk_system_size * 1024L * 1024L) * 0.75;
}

FResult TaskManager::parse_vm_xml(const std::string& xml_file_path, ParseVmXmlParams& params) {
    std::string file_content;
    boost::filesystem::load_string_file(xml_file_path, file_content);
    if (file_content.empty()) {
        return {E_DEFAULT, util::GetFileNameFromPath(xml_file_path) + " file content is empty"};
    }

    tinyxml2::XMLDocument doc;
    tinyxml2::XMLError err = doc.Parse(file_content.c_str());
    if (err != tinyxml2::XML_SUCCESS) {
        return {E_DEFAULT, util::GetFileNameFromPath(xml_file_path) + " file(xml) parse error"};
    }

    tinyxml2::XMLElement* root = doc.RootElement();
    // task_id
    tinyxml2::XMLElement* el_name = root->FirstChildElement("name");
    params.task_id = el_name->GetText();

    // os
    tinyxml2::XMLElement* el_os = root->FirstChildElement("os");
    if (el_os) {
        tinyxml2::XMLElement* el_os_loader = el_os->FirstChildElement("loader");
        if (el_os_loader) {
            std::string boot_loader = el_os_loader->GetText();
            if (boot_loader.find("/usr/share/OVMF/OVMF") != std::string::npos) {
                params.bios_mode = "uefi";
            }
        }
    }
    if (params.bios_mode.empty()) {
        params.bios_mode = "legacy";
    }

    // image_name
    tinyxml2::XMLElement* el_devices = root->FirstChildElement("devices");
    tinyxml2::XMLElement* el_disk_system = el_devices->FirstChildElement("disk"); //认为第一块disk为系统盘
    tinyxml2::XMLElement* el_source = el_disk_system->FirstChildElement("source");
    std::string image_file = el_source->Attribute("file");
    params.image_name = util::GetFileNameFromPath(image_file);
    if (params.image_name.find("win") != std::string::npos) {
        params.operation_system = "windows";
    } else {
        params.operation_system = "generic";
    }

    // disk_file_name
    el_disk_system = el_disk_system->NextSiblingElement("disk"); //认为第二块disk为数据盘
    if (el_disk_system) {
        tinyxml2::XMLElement* el_source2 = el_disk_system->FirstChildElement("source");
        std::string data_file = el_source2->Attribute("file");
        params.data_file_name = util::GetFileNameFromPath(data_file);
    }

    // cpu
    tinyxml2::XMLElement* el_cpu = root->FirstChildElement("cpu");
    tinyxml2::XMLElement* el_topology = el_cpu->FirstChildElement("topology");
    params.cpu_sockets = el_topology->IntAttribute("sockets");
    params.cpu_cores = el_topology->IntAttribute("cores");
    params.cpu_threads = el_topology->IntAttribute("threads");

    // gpu
    tinyxml2::XMLElement* ele_hostdev = el_devices->FirstChildElement("hostdev");
    std::string cur_gpu_id;
    while (ele_hostdev != nullptr) {
        tinyxml2::XMLElement* ele_source = ele_hostdev->FirstChildElement("source");
        if (ele_source != nullptr) {
            tinyxml2::XMLElement* ele_address = ele_source->FirstChildElement("address");
            if (ele_address != nullptr) {
                std::string str_bus = ele_address->Attribute("bus");
                std::string str_slot = ele_address->Attribute("slot");
                std::string str_function = ele_address->Attribute("function");

                auto pos1 = str_bus.find("0x");
                if (pos1 != std::string::npos)
                    str_bus = str_bus.substr(2);

                auto pos2 = str_slot.find("0x");
                if (pos2 != std::string::npos)
                    str_slot = str_slot.substr(2);

                auto pos3 = str_function.find("0x");
                if (pos3 != std::string::npos)
                    str_function = str_function.substr(2);

                std::string device_id = str_bus + ":" + str_slot + "." + str_function;

                if (atoi(str_function.c_str()) == 0) {
                    cur_gpu_id = device_id;
                }

                params.gpus[cur_gpu_id].push_back(device_id);
            }
        }

        ele_hostdev = ele_hostdev->NextSiblingElement("hostdev");
    }

    // mem
    tinyxml2::XMLElement* el_memory = root->FirstChildElement("memory");
    if (el_memory != nullptr) {
        const char* str_memory = el_memory->GetText();
        params.mem_size = atol(str_memory);
    }

    // disk
    tinyxml2::XMLElement* ele_disk = el_devices->FirstChildElement("disk");
    ele_disk = ele_disk->NextSiblingElement("disk"); // 跳过第一块disk（系统盘）
    int disk_index = 1;
    while (ele_disk != nullptr) {
        tinyxml2::XMLElement* ele_source = ele_disk->FirstChildElement("source");
        std::string disk_file = ele_source->Attribute("file");
        std::string disk_filename = util::GetFileNameFromPath(disk_file);
        std::string file_ext = util::GetFileExt(disk_filename);
        if (file_ext == "qcow2") {
            std::string cmd = "qemu-img info " + disk_file + " | grep \"virtual size\" | awk -F '(' '{print $2}' | awk -F ' ' '{print $1}'";
            std::string s_disk_size = run_shell(cmd.c_str());
            params.disks[disk_index] = atol(s_disk_size.c_str()) / 1024L;
            disk_index++;
        }

        ele_disk = ele_disk->NextSiblingElement("disk");
    }

    // vnc
    tinyxml2::XMLElement* ele_graphics = el_devices->FirstChildElement("graphics");
    while (ele_graphics != nullptr) {
        std::string graphics_type = ele_graphics->Attribute("type");
        if (graphics_type == "vnc") {
            std::string vnc_port = ele_graphics->Attribute("port");
            std::string vnc_pwd = ele_graphics->Attribute("passwd");
            params.vnc_port = vnc_port.empty() ? -1 : atoi(vnc_port.c_str());
            params.vnc_password = vnc_pwd;
        }
        ele_graphics = ele_graphics->NextSiblingElement("graphics");
    }

    return {E_SUCCESS, "ok"};
}

FResult TaskManager::check_image(const std::string &image_name) {
    if (image_name.empty() || image_name.substr(image_name.size() - 5) != "qcow2") {
        return {E_DEFAULT, "image_name is empty or image_name is invalid"};
    }

    boost::system::error_code error_code;
    boost::filesystem::path image_path("/data/" + image_name);
    if (!boost::filesystem::exists(image_path, error_code) || error_code) {
        std::vector<std::string> images;
        ImageManager::instance().ListAllImages(images);
        auto iter = std::find(images.begin(), images.end(), image_name);
        if (iter != images.end()) {
            image_path = "/nfs_dbc_images/" + image_name;
        } else {
            return {E_DEFAULT, "image not exist"};
        }
    }

    if (!boost::filesystem::is_regular_file(image_path, error_code) || error_code) {
        return {E_DEFAULT, "image is not a regular file"};
    }

    return FResultOK;
}

FResult TaskManager::check_data_image(const std::string& data_image_name) {
    if (!data_image_name.empty()) {
        boost::filesystem::path image_path("/data/" + data_image_name);
        if (!boost::filesystem::exists(image_path)) {
            return {E_DEFAULT, "image not exist"};
        }

        if (!boost::filesystem::is_regular_file(image_path)) {
            return {E_DEFAULT, "image is not a regular file"};
        }
    }
    return FResultOK;
}

FResult TaskManager::check_cpu(int32_t sockets, int32_t cores, int32_t threads) {
    int32_t cpu_cores = sockets * cores * threads;
    if (cpu_cores <= 0 || cpu_cores > SystemInfo::instance().cpuinfo().total_cores ||
        sockets > SystemInfo::instance().cpuinfo().physical_cores ||
        cores > SystemInfo::instance().cpuinfo().physical_cores_per_cpu ||
        threads > SystemInfo::instance().cpuinfo().threads_per_cpu) {
        return {E_DEFAULT, "cpu config is invalid"};
    }

    return FResultOK;
}

FResult TaskManager::check_gpu(const std::map<std::string, std::list<std::string>> &gpus) {
    std::map<std::string, gpu_info> can_use_gpu = SystemInfo::instance().gpuinfo();
    auto tasks = TaskInfoMgr::instance().getTasks();
    for (auto& it : tasks) {
        if (it.second->status != TS_ShutOff && it.second->status < TS_Error) {
            auto task_resource = TaskResourceMgr::instance().getTaskResource(it.first);
            if (task_resource != nullptr) {
                const std::map<std::string, std::list<std::string>> &gpus = task_resource->gpus;
                for (auto &it1: gpus) {
                    can_use_gpu.erase(it1.first);
                }
            }
        }
    }

    for (auto& it : gpus) {
        auto it_gpu = can_use_gpu.find(it.first);
        if (it_gpu == can_use_gpu.end()) {
            return {E_DEFAULT, "gpu " + it.first + " is already in use"};
        }

        for (auto& it_device : it.second) {
            std::list<std::string>& can_use_devices = it_gpu->second.devices;
            auto found = std::find(can_use_devices.begin(), can_use_devices.end(), it_device);
            if (found == can_use_devices.end()) {
                return {E_DEFAULT, "gpu device " + it_device + " not found in system"};
            }
        }
    }

    return FResultOK;
}

FResult TaskManager::check_disk(const std::map<int32_t, uint64_t> &disks) {
    uint64_t disk_available = SystemInfo::instance().diskinfo().disk_available - g_disk_system_size * 1024L * 1024L;
    uint64_t need_size = 0;
    for (auto& it : disks) {
        need_size += it.second;
    }

    if (need_size > disk_available)
        return {E_DEFAULT, "no enough disk, can available size: " + std::to_string(disk_available)};
    else
        return FResultOK;
}

FResult TaskManager::check_operation_system(const std::string& os) {
    if (os.empty()) return FResultOK;
    if (os == "generic") return FResultOK;
    if (os.find("ubuntu") != std::string::npos) return FResultOK;
    if (os.find("win") != std::string::npos) return FResultOK;
    return {E_DEFAULT, "unsupported operation system"};
}

FResult TaskManager::check_bios_mode(const std::string& bios_mode) {
    if (bios_mode.empty()) return FResultOK;
    if (bios_mode == "legacy") return FResultOK;
    if (bios_mode == "uefi") return FResultOK;
    return {E_DEFAULT, "bios mode only supported [legacy] or [uefi]"};
}

FResult TaskManager::parse_create_snapshot_params(const std::string &additional, const std::string &task_id, std::shared_ptr<dbc::snapshotInfo> &info) {
    if (additional.empty()) {
        return {E_DEFAULT, "additional is empty"};
    }

    rapidjson::Document doc;
    doc.Parse(additional.c_str());
    if (!doc.IsObject()) {
        return {E_DEFAULT, "additional(json) parse failed"};
    }

    std::string snapshot_name, description;
    JSON_PARSE_STRING(doc, "snapshot_name", snapshot_name)
    JSON_PARSE_STRING(doc, "description", description)

    if (snapshot_name.empty()) {
        return {E_DEFAULT, "snapshot_name is not specified"};
    }

    if (SnapshotManager::instance().getTaskSnapshot(task_id, snapshot_name, false) != nullptr) {
        return {E_DEFAULT, "snapshot_name is already exist"};
    }

    if (description.empty()) {
        return {E_DEFAULT, "snapshot description is not specified"};
    }

    info->__set_name(snapshot_name);
    info->__set_description(description);

    std::map<std::string, domainDiskInfo> ddInfos;
    if (!m_vm_client.ListDomainDiskInfo(task_id, ddInfos) || ddInfos.empty()) {
        return {E_DEFAULT, "can not get vm disk info"};
    }
    for (const auto & disk : ddInfos) {
        LOG_INFO << "list domain disk name:" << disk.second.targetDev << ", driver name:" << disk.second.driverName
        << ", driver type:" << disk.second.driverType << ", source file:" << disk.second.sourceFile << ", target bus:" << disk.second.targetBus;
    }

    std::vector<dbc::snapshotDiskInfo> disks;
    if(doc.HasMember("disks")) {
        const rapidjson::Value& obj_disks = doc["disks"];
        if (!obj_disks.IsArray()) {
            return {E_DEFAULT, "additional disks must be an array"};
        }
        for (size_t i = 0; i < obj_disks.Size(); i++) {
            const rapidjson::Value& v = obj_disks[i];
            if (!v.IsObject()) {
                return {E_DEFAULT, "parse additional disks item json failed"};
            }
            dbc::snapshotDiskInfo disk;
            if (!v.HasMember("disk_name") || !v["disk_name"].IsString()) {
                return {E_DEFAULT, "parse additional disk_name json failed"};
            }
            disk.__set_name(v["disk_name"].GetString());
            if (ddInfos.find(disk.name) == ddInfos.end()) {
                return {E_DEFAULT, "additional disk_name not exist in task:" + task_id};
            }
            if (!v.HasMember("snapshot_type")) {
                disk.__set_snapshot("external");
            } else {
                if (!v["snapshot_type"].IsString()) {
                    return {E_DEFAULT, "parse additional disks item snapshot_type json failed"};
                }
                disk.__set_snapshot(v["snapshot_type"].GetString());
                if (disk.snapshot.empty()) {
                    return {E_DEFAULT, "additional disks item snapshot_type can not empty"};
                }
                if (!(disk.snapshot == "external" || disk.snapshot == "no" || disk.snapshot == "internal")) {
                    return {E_DEFAULT, "invalid additional disks item snapshot_type value:" + disk.snapshot};
                }
            }
            if (!v.HasMember("driver")) {
                disk.__set_driver_type("qcow2");
            } else {
                if (!v["driver"].IsString()) {
                    return {E_DEFAULT, "parse additional disks item driver json failed"};
                }
                disk.__set_driver_type(v["driver"].GetString());
                if (disk.driver_type.empty()) {
                    return {E_DEFAULT, "additional disks item driver can not empty"};
                }
                if (disk.driver_type != "qcow2") {
                    return {E_DEFAULT, "additional disks item driver only support qcow2"};
                }
            }
            if (v.HasMember("snapshot_file_name")) {
                if (!v["snapshot_file_name"].IsString()) {
                    return {E_DEFAULT, "parse additional disks item snapshot_file_name json failed"};
                }
                std::string snapshot_file_name = v["snapshot_file_name"].GetString();
                if (snapshot_file_name.find("/") != std::string::npos || snapshot_file_name.find(".qcow2") == std::string::npos) {
                    return {E_DEFAULT, "invalid snapshot_file_name, use format: xxx.qcow2"};
                }
                std::string snapshot_file_path = "/data/" + snapshot_file_name;
                boost::filesystem::path disk_source_file(snapshot_file_path);
                if (disk_source_file.extension().string() != ".qcow2") {
                    return {E_DEFAULT, "disk snapshot_file_name must end with .qcow2, such as xxx.qcow2"};
                }
                boost::system::error_code err;
                if (boost::filesystem::exists(disk_source_file, err)) {
                    return {E_DEFAULT, "disks item snapshot_file_name already exist"};
                }
                // if (err) {
                //     return {E_DEFAULT, "check disk item snapshot_file_name error:" + err.message()};
                // }
                disk.__set_source_file(snapshot_file_path);
            } else {
                disk.__set_source_file("/data/" + task_id + "_" + snapshot_name + "_" + disk.name + ".qcow2");
            }
            disks.push_back(disk);
        }
    } else {
        for (const auto &disk : ddInfos) {
            if (disk.second.driverType != "qcow2") {
                continue;
            }
            dbc::snapshotDiskInfo sdInfo;
            sdInfo.__set_name(disk.second.targetDev);
            sdInfo.__set_driver_type(disk.second.driverType);
            sdInfo.__set_snapshot("external");
            sdInfo.__set_source_file("/data/" + task_id + "_" + snapshot_name + "_" + sdInfo.name + ".qcow2");
            disks.push_back(sdInfo);
        }
    }

    if (disks.empty()) {
        return {E_DEFAULT, "disks is empty"};
    }
    info->__set_disks(disks);
    for (auto& disk : info->disks) {
        LOG_INFO << "parse additional disk name:" << disk.name << ", driver type:" << disk.driver_type
                 << ", snapshot type:" << disk.snapshot << ", source file:" << disk.source_file;
        if (disk.snapshot == "external" && disk.source_file.empty()) {
            disk.source_file = "/data/" + task_id + "_" + snapshot_name + "_" + disk.name + ".qcow2";
        }
    }

    return FResultOK;
}

std::shared_ptr<dbc::TaskInfo> TaskManager::findTask(const std::string& wallet, const std::string &task_id) {
    std::shared_ptr<dbc::TaskInfo> taskinfo = nullptr;
    auto renttask = WalletRentTaskMgr::instance().getRentTask(wallet);
    if (renttask != nullptr) {
        for (auto &id: renttask->task_ids) {
            if (id == task_id) {
                taskinfo = TaskInfoMgr::instance().getTaskInfo(task_id);
                break;
            }
        }
    }

    return taskinfo;
}

FResult TaskManager::startTask(const std::string& wallet, const std::string &task_id) {
    auto taskinfo = TaskInfoMgr::instance().getTaskInfo(task_id);
    if (taskinfo == nullptr) {
        return {E_DEFAULT, "task_id not exist"};
    }

    if (!m_vm_client.IsExistDomain(task_id)) {
        return {E_DEFAULT, "domain not exist"};
    }

    //TODO： check task resource


    virDomainState vm_status = m_vm_client.GetDomainStatus(task_id);
    if (vm_status == VIR_DOMAIN_NOSTATE || vm_status == VIR_DOMAIN_SHUTOFF) {
        taskinfo->__set_status(TS_Starting);
        taskinfo->__set_last_start_time(time(nullptr));
        TaskInfoMgr::instance().update(taskinfo);

        ETaskEvent ev;
        ev.task_id = task_id;
        ev.op = T_OP_Start;
        add_process_task(ev);
        return FResultOK;
    } else {
        return {E_DEFAULT, "task is " + vm_status_string(vm_status)};
    }
}

FResult TaskManager::stopTask(const std::string& wallet, const std::string &task_id) {
    auto taskinfo = TaskInfoMgr::instance().getTaskInfo(task_id);
    if (taskinfo == nullptr) {
        return {E_DEFAULT, "task_id not exist"};
    }

    if (!m_vm_client.IsExistDomain(task_id)) {
        return {E_DEFAULT, "domain not exist"};
    }

    virDomainState vm_status = m_vm_client.GetDomainStatus(task_id);
    if (vm_status == VIR_DOMAIN_NOSTATE || vm_status == VIR_DOMAIN_RUNNING || vm_status == VIR_DOMAIN_PAUSED) {
        taskinfo->__set_status(TS_Stopping);
        taskinfo->__set_last_stop_time(time(nullptr));
        TaskInfoMgr::instance().update(taskinfo);

        ETaskEvent ev;
        ev.task_id = task_id;
        ev.op = T_OP_Stop;
        add_process_task(ev);
        return FResultOK;
    } else {
        return {E_DEFAULT, "task is " + vm_status_string(vm_status)};
    }
}

FResult TaskManager::restartTask(const std::string& wallet, const std::string &task_id) {
    auto taskinfo = TaskInfoMgr::instance().getTaskInfo(task_id);
    if (taskinfo == nullptr) {
        return {E_DEFAULT, "task_id not exist"};
    }

    if (!m_vm_client.IsExistDomain(task_id)) {
        return {E_DEFAULT, "domain not exist"};
    }

    // TODO: check task resource


    virDomainState vm_status = m_vm_client.GetDomainStatus(task_id);
    if (vm_status == VIR_DOMAIN_NOSTATE || vm_status == VIR_DOMAIN_SHUTOFF || vm_status == VIR_DOMAIN_RUNNING) {
        taskinfo->__set_status(TS_Restarting);
        taskinfo->__set_last_start_time(time(nullptr));
        TaskInfoMgr::instance().update(taskinfo);

        ETaskEvent ev;
        ev.task_id = task_id;
        ev.op = T_OP_ReStart;
        add_process_task(ev);
        return FResultOK;
    } else {
        return {E_DEFAULT, "task is " + vm_status_string(vm_status)};
    }
}

FResult TaskManager::resetTask(const std::string& wallet, const std::string &task_id) {
    auto taskinfo = TaskInfoMgr::instance().getTaskInfo(task_id);
    if (taskinfo == nullptr) {
        return {E_DEFAULT, "task_id not exist"};
    }

    if (!m_vm_client.IsExistDomain(task_id)) {
        return {E_DEFAULT, "domain not exist"};
    }

    virDomainState vm_status = m_vm_client.GetDomainStatus(task_id);
    if (vm_status == VIR_DOMAIN_RUNNING) {
        taskinfo->__set_status(TS_Resetting);
        taskinfo->__set_last_start_time(time(nullptr));
        TaskInfoMgr::instance().update(taskinfo);

        ETaskEvent ev;
        ev.task_id = task_id;
        ev.op = T_OP_Reset;
        add_process_task(ev);
        return FResultOK;
    } else {
        return {E_DEFAULT, "task is " + vm_status_string(vm_status)};
    }
}

FResult TaskManager::deleteTask(const std::string& wallet, const std::string &task_id) {
    auto taskinfo = TaskInfoMgr::instance().getTaskInfo(task_id);
    if (taskinfo == nullptr) {
        return {E_DEFAULT, "task_id not exist"};
    }

    virDomainState vm_status = m_vm_client.GetDomainStatus(task_id);
    if (vm_status == VIR_DOMAIN_NOSTATE || vm_status == VIR_DOMAIN_SHUTOFF ||
        vm_status == VIR_DOMAIN_RUNNING || vm_status == VIR_DOMAIN_PAUSED) {
        taskinfo->__set_status(TS_Deleting);
        taskinfo->__set_last_stop_time(time(nullptr));
        TaskInfoMgr::instance().update(taskinfo);

        ETaskEvent ev;
        ev.task_id = task_id;
        ev.op = T_OP_Delete;
        add_process_task(ev);
        return FResultOK;
    } else {
        return {E_DEFAULT, "task is " + vm_status_string(vm_status)};
    }
}

FResult TaskManager::getTaskLog(const std::string &task_id, ETaskLogDirection direction, int32_t nlines,
                                std::string &log_content) {
    auto taskinfo = TaskInfoMgr::instance().getTaskInfo(task_id);
    if (taskinfo == nullptr) {
        log_content = "";
        return {E_DEFAULT, "task_id not exist"};
    } else {
        return m_vm_client.GetDomainLog(task_id, direction, nlines, log_content);
    }
}

void TaskManager::listAllTask(const std::string& wallet, std::vector<std::shared_ptr<dbc::TaskInfo>> &vec) {
    auto renttask = WalletRentTaskMgr::instance().getRentTask(wallet);
    if (renttask != nullptr) {
        std::vector<std::string> ids = renttask->task_ids;
        for (auto &it_id: ids) {
            auto taskinfo = TaskInfoMgr::instance().getTaskInfo(it_id);
            if (taskinfo != nullptr) {
                vec.push_back(taskinfo);
            }
        }
    }
}

void TaskManager::listRunningTask(const std::string& wallet, std::vector<std::shared_ptr<dbc::TaskInfo>> &vec) {
    auto renttask = WalletRentTaskMgr::instance().getRentTask(wallet);
    if (renttask != nullptr) {
        std::vector<std::string> ids = renttask->task_ids;
        for (auto &it_id: ids) {
            auto taskinfo = TaskInfoMgr::instance().getTaskInfo(it_id);
            if (taskinfo != nullptr && taskinfo->status == TS_Running) {
                vec.push_back(taskinfo);
            }
        }
    }
}

int32_t TaskManager::getRunningTasksSize(const std::string& wallet) {
    int32_t count = 0;
    auto renttask = WalletRentTaskMgr::instance().getRentTask(wallet);
    for (auto& it_id : renttask->task_ids) {
        auto taskinfo = TaskInfoMgr::instance().getTaskInfo(it_id);
        if (taskinfo != nullptr && taskinfo->status == TS_Running) {
            count += 1;
        }
    }

    return count;
}

ETaskStatus TaskManager::getTaskStatus(const std::string &task_id) {
    auto taskinfo = TaskInfoMgr::instance().getTaskInfo(task_id);
    if (taskinfo == nullptr) {
        return TS_ShutOff;
    } else {
        ETaskStatus ts = (ETaskStatus) taskinfo->status;
        virDomainState vm_status = m_vm_client.GetDomainStatus(task_id);
        if (ts == TS_Running && vm_status == VIR_DOMAIN_SHUTOFF) {
            ETaskEvent ev;
            ev.task_id = task_id;
            ev.op = T_OP_Start;
            add_process_task(ev);
        } else if (ts == TS_ShutOff && vm_status == VIR_DOMAIN_RUNNING) {
            ETaskEvent ev;
            ev.task_id = task_id;
            ev.op = T_OP_Stop;
            add_process_task(ev);
        }

        return ts;
    }
}

void TaskManager::deleteAllCheckTasks() {
    std::vector<std::string> check_ids;
    std::vector<std::string> ids = TaskInfoMgr::instance().getAllTaskId();
    for (auto& id : ids) {
        if (id.find("vm_check_") != std::string::npos) {
            check_ids.push_back(id);
        }
    }

    for (auto& task_id : check_ids) {
        delete_task(task_id);
    }
}

void TaskManager::deleteOtherCheckTasks(const std::string& wallet) {
    auto renttasks = WalletRentTaskMgr::instance().getRentTasks();
    std::vector<std::string> check_ids;
    for (auto& it : renttasks) {
        if (it.first != wallet) {
            for (auto& id : it.second->task_ids) {
                auto taskinfo = TaskInfoMgr::instance().getTaskInfo(id);
                if (taskinfo != nullptr) {
                    if (taskinfo->task_id.find("vm_check_") != std::string::npos) {
                        check_ids.push_back(taskinfo->task_id);
                    }
                }
            }
        }
    }

    for (auto& task_id : check_ids) {
        delete_task(task_id);
    }
}

std::string TaskManager::createSessionId(const std::string &wallet, const std::vector<std::string>& multisig_signers) {
    auto wallet_session = WalletSessionIDMgr::instance().getSessionIdByWallet(wallet);
    if (wallet_session == nullptr) {
        std::string session_id = util::create_session_id();
        std::shared_ptr<dbc::rent_sessionid> ptr = std::make_shared<dbc::rent_sessionid>();
        ptr->rent_wallet = wallet;
        ptr->session_id = session_id;
        ptr->__set_multisig_signers(multisig_signers);
        WalletSessionIDMgr::instance().addWalletSessionId(ptr);
        return session_id;
    } else {
        return wallet_session->session_id;
    }
}

std::string TaskManager::getSessionId(const std::string &wallet) {
    auto it = WalletSessionIDMgr::instance().getSessionIdByWallet(wallet);
    if (it != nullptr) {
        return it->session_id;
    } else {
        return "";
    }
}

std::string TaskManager::checkSessionId(const std::string &session_id, const std::string &session_id_sign) {
    if (session_id.empty() || session_id_sign.empty())
        return "";

    auto it = WalletSessionIDMgr::instance().findSessionId(session_id);
    if (it == nullptr) {
        return "";
    }

    if (util::verify_sign(session_id_sign, session_id, it->rent_wallet)) {
        return it->rent_wallet;
    }

    for (auto& it_wallet : it->multisig_signers) {
        if (util::verify_sign(session_id_sign, session_id, it_wallet)) {
            return it->rent_wallet;
        }
    }

    return "";
}

void TaskManager::listTaskDiskInfo(const std::string& task_id, std::map<std::string, domainDiskInfo>& disks) {
    auto taskinfo = TaskInfoMgr::instance().getTaskInfo(task_id);
    if (taskinfo != nullptr) {
        m_vm_client.ListDomainDiskInfo(task_id, disks);
    }
}

FResult TaskManager::createSnapshot(const std::string& wallet, const std::string &additional, const std::string& task_id) {
    auto taskinfo = TaskInfoMgr::instance().getTaskInfo(task_id);
    if (taskinfo == nullptr) {
        return {E_DEFAULT, "task_id not exist"};
    }

    if (!m_vm_client.IsExistDomain(task_id)) {
        return {E_DEFAULT, "domain not exist"};
    }

    {
        std::shared_ptr<dbc::snapshotInfo> temp = SnapshotManager::instance().getCreatingSnapshot(task_id);
        if (temp != nullptr && !temp->__isset.error_code) {
            return {E_DEFAULT, "task snapshot is busy, please wait a moment"};
        }
    }
    std::shared_ptr<dbc::snapshotInfo> sInfo = std::make_shared<dbc::snapshotInfo>();
    FResult fret = parse_create_snapshot_params(additional, task_id, sInfo);
    if (std::get<0>(fret) != E_SUCCESS) {
        return fret;
    }

    //TODO： check task resource
    SnapshotManager::instance().addCreatingSnapshot(task_id, sInfo);

    virDomainState vm_status = m_vm_client.GetDomainStatus(task_id);
    if (/*vm_status == VIR_DOMAIN_RUNNING || */vm_status == VIR_DOMAIN_SHUTOFF) {
        taskinfo->__set_status(TS_CreatingSnapshot);
        taskinfo->__set_last_stop_time(time(nullptr));
        TaskInfoMgr::instance().update(taskinfo);

        ETaskEvent ev;
        ev.task_id = task_id;
        ev.op = T_OP_CreateSnapshot;
        add_process_task(ev);
        return FResultOK;
    } else {
        return {E_DEFAULT, "task is " + vm_status_string(vm_status) + ", please save the job and shutdown task before creating a snapshot"};
    }
}

FResult TaskManager::deleteSnapshot(const std::string& wallet, const std::string &task_id, const std::string& snapshot_name) {
    return {E_DEFAULT, "The function is not implemented yet."};
}

FResult TaskManager::listTaskSnapshot(const std::string& wallet, const std::string& task_id, std::vector<std::shared_ptr<dbc::snapshotInfo>>& snaps) {
    // auto renttask = WalletRentTaskMgr::instance().getRentTask(wallet);
    // if (renttask == nullptr) {
    //     return {E_DEFAULT, "this wallet has none rent task"};
    // }
    // std::shared_ptr<dbc::TaskInfo> taskinfo = nullptr;
    // for (auto& id : renttask->task_ids) {
    //     if (id == task_id) {
    //         taskinfo = TaskInfoMgr::instance().getTaskInfo(task_id);
    //         break;
    //     }
    // }
    // if (taskinfo == nullptr) {
    //     return {E_DEFAULT, "task_id not exist"};
    // }
    SnapshotManager::instance().listTaskSnapshot(task_id, snaps);
    return {E_SUCCESS, "ok"};
}

std::shared_ptr<dbc::snapshotInfo> TaskManager::getTaskSnapshot(const std::string& wallet, const std::string& task_id, const std::string& snapshot_name) {
    // auto renttask = WalletRentTaskMgr::instance().getRentTask(wallet);
    // if (renttask == nullptr) {
    //     return nullptr;
    // }
    // std::shared_ptr<dbc::TaskInfo> taskinfo = nullptr;
    // for (auto& id : renttask->task_ids) {
    //     if (id == task_id) {
    //         taskinfo = TaskInfoMgr::instance().getTaskInfo(task_id);
    //         break;
    //     }
    // }
    // if (taskinfo == nullptr) {
    //     return nullptr;
    // }
    return SnapshotManager::instance().getTaskSnapshot(task_id, snapshot_name);
}

std::shared_ptr<dbc::snapshotInfo> TaskManager::getCreatingSnapshot(const std::string& wallet, const std::string& task_id) {
    // auto renttask = WalletRentTaskMgr::instance().getRentTask(wallet);
    // if (renttask == nullptr) {
    //     return nullptr;
    // }
    // std::shared_ptr<dbc::TaskInfo> taskinfo = nullptr;
    // for (auto& id : renttask->task_ids) {
    //     if (id == task_id) {
    //         taskinfo = TaskInfoMgr::instance().getTaskInfo(task_id);
    //         break;
    //     }
    // }
    // if (taskinfo == nullptr) {
    //     return nullptr;
    // }
    return SnapshotManager::instance().getCreatingSnapshot(task_id);
}

void TaskManager::add_process_task(const ETaskEvent& ev) {
    std::unique_lock<std::mutex> lock(m_process_mtx);
    m_process_tasks.push(ev);
    m_process_cond.notify_one();
}

void TaskManager::process_task_thread_func() {
    while (m_running) {
        ETaskEvent ev;
        {
            std::unique_lock<std::mutex> lock(m_process_mtx);
            m_process_cond.wait(lock, [this] {
                return !m_running || !m_process_tasks.empty();
            });

            if (!m_running)
                break;

            ev = m_process_tasks.front();
            m_process_tasks.pop();
        }

        process_task(ev);
    }
}

void TaskManager::process_task(const ETaskEvent& ev) {
    auto taskinfo = TaskInfoMgr::instance().getTaskInfo(ev.task_id);
    if (taskinfo == nullptr) return;

    int32_t res_code = E_DEFAULT;
    std::string res_msg;
    switch (ev.op) {
        case T_OP_Create:
            process_create(taskinfo);
            break;
        case T_OP_Start:
            process_start(taskinfo);
            break;
        case T_OP_Stop:
            process_stop(taskinfo);
            break;
        case T_OP_ReStart:
            process_restart(taskinfo);
            break;
        case T_OP_Reset:
            process_reset(taskinfo);
            break;
        case T_OP_Delete:
            process_delete(taskinfo);
            break;
        case T_OP_CreateSnapshot:
            process_create_snapshot(taskinfo);
            break;
        default:
            TASK_LOG_ERROR(taskinfo->task_id, "unknown op:" << ev.op);
            break;
    }
}

void TaskManager::process_create(const std::shared_ptr<dbc::TaskInfo>& taskinfo) {
    auto task_resource = TaskResourceMgr::instance().getTaskResource(taskinfo->task_id);
    if (task_resource == nullptr) {
        taskinfo->status = TS_CreateError;
        TaskInfoMgr::instance().update(taskinfo);
        TASK_LOG_ERROR(taskinfo->task_id, "not found task resource");
        return;
    }

    // todo: 解析父快照，并发送给下载模块
    DownloadImageEvent diEvent;
    getNeededBackingImage(taskinfo->image_name, diEvent.images);
    if (!diEvent.images.empty()) {
        diEvent.task_id = taskinfo->task_id;
        std::string _taskid = taskinfo->task_id;
        LOG_INFO << "need download image: " << diEvent.images[0];
        ImageManager::instance().PushDownloadEvent(diEvent, [this, _taskid] () {
            ETaskEvent ev;
            ev.task_id = _taskid;
            ev.op = T_OP_Create;
            add_process_task(ev);
        });
        return;
    }

    ERR_CODE err_code = m_vm_client.CreateDomain(taskinfo->task_id, taskinfo->image_name, taskinfo->data_file_name, task_resource,
                                                 taskinfo->operation_system.find("win") != std::string::npos,
                                                 taskinfo->bios_mode == "uefi");
    if (err_code != E_SUCCESS) {
        taskinfo->status = TS_CreateError;
        TaskInfoMgr::instance().update(taskinfo);
        TASK_LOG_ERROR(taskinfo->task_id, "create domain failed");
    } else {
        std::string local_ip = m_vm_client.GetDomainLocalIP(taskinfo->task_id);
        if (!local_ip.empty()) {
            if (!m_vm_client.SetDomainUserPassword(taskinfo->task_id,
                taskinfo->operation_system.find("win") == std::string::npos ? g_vm_login_username : g_vm_login_username_win, taskinfo->login_password)) {
                m_vm_client.DestroyDomain(taskinfo->task_id);

                taskinfo->status = TS_CreateError;
                TaskInfoMgr::instance().update(taskinfo);
                TASK_LOG_ERROR(taskinfo->task_id, "set domain password failed");
                return;
            }

            if (!create_task_iptable(taskinfo->task_id, taskinfo->ssh_port, taskinfo->rdp_port,
                                     taskinfo->custom_port, local_ip)) {
                m_vm_client.DestroyDomain(taskinfo->task_id);

                taskinfo->status = TS_CreateError;
                TaskInfoMgr::instance().update(taskinfo);
                TASK_LOG_ERROR(taskinfo->task_id, "create task iptable failed");
                return;
            }

            taskinfo->status = TS_Running;
            TaskInfoMgr::instance().update(taskinfo);
            TASK_LOG_INFO(taskinfo->task_id, "create task successful");
        }
        else {
            m_vm_client.DestroyDomain(taskinfo->task_id);

            taskinfo->status = TS_CreateError;
            TaskInfoMgr::instance().update(taskinfo);
            TASK_LOG_ERROR(taskinfo->task_id, "get domain local ip failed");
        }
    }
}

bool TaskManager::create_task_iptable(const std::string &domain_name, const std::string &ssh_port,
                                      const std::string& rdp_port, const std::vector<std::string>& custom_port,
                                      const std::string &vm_local_ip) {
    std::string public_ip = SystemInfo::instance().publicip();
    if (!public_ip.empty() && !vm_local_ip.empty()) {
        shell_add_iptable_to_system(domain_name, public_ip, ssh_port, rdp_port, custom_port, vm_local_ip);

        std::shared_ptr<dbc::task_iptable> iptable = std::make_shared<dbc::task_iptable>();
        iptable->task_id = domain_name;
        iptable->__set_host_ip(public_ip);
        iptable->__set_vm_local_ip(vm_local_ip);
        iptable->__set_ssh_port(ssh_port);
        iptable->__set_rdp_port(rdp_port);
        iptable->__set_custom_port(custom_port);
        TaskIptableMgr::instance().addIptable(iptable);

        TASK_LOG_INFO(domain_name, "transform ssh_port successful, "
                                   "public_ip:" << public_ip << " ssh_port:" << ssh_port
                                   << " rdp_port:" << rdp_port << " custom_port.size:" << custom_port.size()
                                   << " local_ip:" << vm_local_ip);
        return true;
    } else {
        TASK_LOG_ERROR(domain_name, "transform ssh_port failed, public_ip or vm_local_ip is empty: " << public_ip << ":" << vm_local_ip);
        return false;
    }
}

void TaskManager::getNeededBackingImage(const std::string &image_name, std::vector<std::string> &backing_images) {
    std::string cur_image = "/data/" + image_name;
    while (cur_image.find("/data/") != std::string::npos) {
        boost::system::error_code error_code;
        if (!boost::filesystem::exists(cur_image, error_code) || error_code) {
            boost::filesystem::path full_path(cur_image);
            backing_images.push_back(full_path.filename().string());
            break;
        }

        std::string cmd_get_backing_file = "qemu-img info " + cur_image + " | grep -i 'backing file' | awk -F ': ' '{print $2}'";
        cur_image = run_shell(cmd_get_backing_file);
    }
}

void TaskManager::process_start(const std::shared_ptr<dbc::TaskInfo> &taskinfo) {
    ERR_CODE err_code = m_vm_client.StartDomain(taskinfo->task_id);
    if (err_code != E_SUCCESS) {
        taskinfo->status = TS_StartError;
        TaskInfoMgr::instance().update(taskinfo);
        TASK_LOG_ERROR(taskinfo->task_id, "start task failed");
    } else {
        taskinfo->status = TS_Running;
        TaskInfoMgr::instance().update(taskinfo);
        add_iptable_to_system(taskinfo->task_id);
        TASK_LOG_INFO(taskinfo->task_id, "start task successful");
    }
}

void TaskManager::process_stop(const std::shared_ptr<dbc::TaskInfo> &taskinfo) {
    ERR_CODE err_code = m_vm_client.DestroyDomain(taskinfo->task_id);
    if (err_code != E_SUCCESS) {
        taskinfo->status = TS_StopError;
        TaskInfoMgr::instance().update(taskinfo);
        TASK_LOG_ERROR(taskinfo->task_id, "stop task failed");
    }
    else {
        taskinfo->status = TS_ShutOff;
        TaskInfoMgr::instance().update(taskinfo);
        remove_iptable_from_system(taskinfo->task_id);
        TASK_LOG_INFO(taskinfo->task_id, "stop task successful");
    }
}

void TaskManager::process_restart(const std::shared_ptr<dbc::TaskInfo> &taskinfo) {
    virDomainState vm_status = m_vm_client.GetDomainStatus(taskinfo->task_id);
    if (vm_status == VIR_DOMAIN_SHUTOFF) {
        ERR_CODE err_code = m_vm_client.StartDomain(taskinfo->task_id);
        if (err_code != E_SUCCESS) {
            taskinfo->status = TS_RestartError;
            TaskInfoMgr::instance().update(taskinfo);
            TASK_LOG_ERROR(taskinfo->task_id, "restart task failed");
        }
        else {
            taskinfo->status = TS_Running;
            TaskInfoMgr::instance().update(taskinfo);
            add_iptable_to_system(taskinfo->task_id);
            TASK_LOG_INFO(taskinfo->task_id, "restart task successful");
        }
    } else if (vm_status == VIR_DOMAIN_RUNNING) {
        ERR_CODE err_code = m_vm_client.RebootDomain(taskinfo->task_id);
        if (err_code != E_SUCCESS) {
            taskinfo->status = TS_RestartError;
            TaskInfoMgr::instance().update(taskinfo);
            TASK_LOG_ERROR(taskinfo->task_id, "restart task failed");
        }
        else {
            taskinfo->status = TS_Running;
            TaskInfoMgr::instance().update(taskinfo);
            add_iptable_to_system(taskinfo->task_id);
            TASK_LOG_INFO(taskinfo->task_id, "restart task successful");
        }
    }
}

void TaskManager::process_reset(const std::shared_ptr<dbc::TaskInfo> &taskinfo) {
    ERR_CODE err_code = m_vm_client.ResetDomain(taskinfo->task_id);
    if (err_code != E_SUCCESS) {
        taskinfo->status = TS_ResetError;
        TaskInfoMgr::instance().update(taskinfo);
        TASK_LOG_ERROR(taskinfo->task_id, "reset task failed");
    }
    else {
        taskinfo->status = TS_Running;
        TaskInfoMgr::instance().update(taskinfo);
        TASK_LOG_INFO(taskinfo->task_id, "reset task successful");
    }
}

void TaskManager::process_delete(const std::shared_ptr<dbc::TaskInfo> &taskinfo) {
    delete_task(taskinfo->task_id);
}

void TaskManager::process_create_snapshot(const std::shared_ptr<dbc::TaskInfo>& taskinfo) {
    std::shared_ptr<dbc::snapshotInfo> info = SnapshotManager::instance().getCreatingSnapshot(taskinfo->task_id);
    if (info == nullptr) {
        LOG_ERROR << "can not find snapshot:" << info->name << " info when creating a snapshot on vm:" << taskinfo->task_id;
        return;
    }
    FResult result = m_vm_client.CreateSnapshot(taskinfo->task_id, info);
    if (std::get<0>(result) != E_SUCCESS) {
        taskinfo->status = TS_CreateSnapshotError;
        TaskInfoMgr::instance().update(taskinfo);
        TASK_LOG_ERROR(taskinfo->task_id, "create task snapshot:" << info->name << " failed");
        SnapshotManager::instance().updateCreatingSnapshot(taskinfo->task_id, std::get<0>(result), std::get<1>(result));
    } else {
        // task will be closed after the snapshot is created whether it is running or not.
        taskinfo->status = TS_ShutOff;
        TaskInfoMgr::instance().update(taskinfo);
        remove_iptable_from_system(taskinfo->task_id);
        TASK_LOG_INFO(taskinfo->task_id, "create task snapshot:" << info->name << " successful, description:" << info->description);
        SnapshotManager::instance().delCreatingSnapshot(taskinfo->task_id);
        std::shared_ptr<dbc::snapshotInfo> newInfo = m_vm_client.GetDomainSnapshot(taskinfo->task_id, info->name);
        if (newInfo) {
            SnapshotManager::instance().addTaskSnapshot(taskinfo->task_id, newInfo);
        } else {
            TASK_LOG_ERROR(taskinfo->task_id, "can not find snapshot:" << info->name);
        }
    }
}

void TaskManager::prune_task_thread_func() {
    while (m_running) {
        sleep(60);
        if (!m_running) break;

        shell_remove_reject_iptable_from_system();

        std::string machine_status = m_httpclient.request_machine_status();
        if (machine_status.empty()) continue;

        auto wallet_renttasks = WalletRentTaskMgr::instance().getRentTasks();
        for (auto &it: wallet_renttasks) {
            if (machine_status == "addingCustomizeInfo" || machine_status == "distributingOrder" ||
                machine_status == "committeeVerifying" || machine_status == "committeeRefused") {
                std::vector<std::string> ids = it.second->task_ids;
                for (auto &task_id: ids) {
                    if (task_id.find("vm_check_") == std::string::npos) {
                        close_task(task_id);
                    }
                }
            } else if (machine_status == "waitingFulfill" || machine_status == "online") {
                std::vector<std::string> ids = it.second->task_ids;
                for (auto &task_id: ids) {
                    if (task_id.find("vm_check_") == std::string::npos) {
                        close_task(task_id);
                    } else {
                        delete_task(task_id);
                    }
                }
            } else if (machine_status == "creating" || machine_status == "rented") {
                std::vector<std::string> ids = it.second->task_ids;
                for (auto &task_id: ids) {
                    if (task_id.find("vm_check_") != std::string::npos) {
                        delete_task(task_id);
                    }
                }
            }

            if (machine_status == "waitingFulfill" || machine_status == "online" ||
                machine_status == "creating" || machine_status == "rented") {
                int64_t rent_end = m_httpclient.request_rent_end(it.first);
                if (rent_end <= 0) {
                    std::vector<std::string> ids = it.second->task_ids;
                    for (auto &task_id: ids) {
                        close_task(task_id);
                    }

                    // 1小时出120个块
                    int64_t wallet_rent_end = it.second->rent_end;
                    int64_t reserve_end = wallet_rent_end + 120 * 24 * 10; //保留10天
                    int64_t cur_block = m_httpclient.request_cur_block();
                    if (reserve_end > cur_block) {
                        ids = it.second->task_ids;
                        for (auto &task_id: ids) {
                            delete_task(task_id);
                        }
                    }
                } else {
                    if (rent_end > it.second->rent_end) {
                        it.second->rent_end = rent_end;
                        WalletRentTaskMgr::instance().updateRentEnd(it.first, rent_end);
                    }
                }
            }
        }
    }
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

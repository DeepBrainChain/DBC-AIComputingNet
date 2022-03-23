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
#include "VxlanManager.h"

FResult TaskManager::init() {
    m_httpclient.connect_chain();

    if (!VmClient::instance().Init()) {
        return FResult(ERR_ERROR, "connect libvirt tcp service failed");
    }

    if (!TaskInfoMgr::instance().init()) {
        return FResult(ERR_ERROR, "task info manager init failed");
    }

    if (!TaskIptableMgr::instance().init()) {
        return FResult(ERR_ERROR, "task iptable manager init failed");
    }

    if (!WalletSessionIDMgr::instance().init()) {
        return FResult(ERR_ERROR, "wallet sessionid manager init failed");
    }

    if (!WalletRentTaskMgr::instance().init()) {
        return FResult(ERR_ERROR, "wallet renttask manager init failed;");
    }

    auto tasks = TaskInfoMgr::instance().getTasks();
    for (auto& it : tasks) {
        dbclog::instance().add_task_log_backend(it.second->task_id);
    }

    if (!restore_tasks()) {
        return FResult(ERR_ERROR, "restore tasks failed");
    }

    shell_remove_reject_iptable_from_system();

    std::vector<std::string> taskids = TaskInfoMgr::instance().getAllTaskId();
    TaskResourceMgr::instance().init(taskids);
    SnapshotManager::instance().init(taskids);

	m_running = true;
	if (m_process_thread == nullptr) {
		m_process_thread = new std::thread(&TaskManager::process_task_thread_func, this);
	}

	if (m_prune_thread == nullptr) {
		m_prune_thread = new std::thread(&TaskManager::prune_task_thread_func, this);
	}

    return FResultSuccess;
}

void TaskManager::exit() {
	m_running = false;
	m_process_cond.notify_all();
	if (m_process_thread != nullptr && m_process_thread->joinable()) {
		m_process_thread->join();
	}
	delete m_process_thread;
	m_process_thread = nullptr;

	m_prune_cond.notify_all();
	if (m_prune_thread != nullptr && m_prune_thread->joinable()) {
		m_prune_thread->join();
	}
	delete m_prune_thread;
	m_prune_thread = nullptr;
}

FResult TaskManager::listImages(const std::shared_ptr<dbc::node_list_images_req_data>& data,
                                const AuthoriseResult& result, std::vector<std::string> &images) {
    try {
        ImageServer imgsvr;
        imgsvr.from_string(data->image_server);

        if (!result.success) {
            ImageMgr::instance().ListLocalShareImages(imgsvr, images);
        } else {
            ImageMgr::instance().ListWalletLocalShareImages(result.rent_wallet, imgsvr, images);
        }

        return FResultSuccess;
    } catch (std::exception& e) {
        return FResultSuccess;
    }
}

FResult TaskManager::downloadImage(const std::string& wallet,
                                   const std::shared_ptr<dbc::node_download_image_req_data>& data) {
    if (data->image_server.empty()) {
        return FResult(ERR_ERROR, "no image_server");
    }

    rapidjson::Document doc;
    doc.Parse(data->additional.c_str());
    if (!doc.IsObject()) {
        return FResult(ERR_ERROR, "additional parse failed");
    }
    std::string image_filename;
    JSON_PARSE_STRING(doc, "image_filename", image_filename)
    if (image_filename.empty()) {
        return FResult(ERR_ERROR, "additional no image_filename");
    }

	std::string local_dir;
	JSON_PARSE_STRING(doc, "local_dir", local_dir)
	if (local_dir.empty()) {
		return FResult(ERR_ERROR, "additional no local_dir");
	}

    std::vector<std::string> images;
    ImageServer imgsvr;
    imgsvr.from_string(data->image_server);
    ImageManager::instance().ListShareImages(imgsvr, images);
    auto iter = std::find(images.begin(), images.end(), image_filename);
    if (iter == images.end()) {
        return FResult(ERR_ERROR, "image:" + image_filename + " not exist");
    }

    if (ImageManager::instance().IsDownloading(image_filename)) {
        return FResult(ERR_ERROR, "image:" + image_filename + " in downloading");
    }

    ImageManager::instance().Download(image_filename, local_dir, imgsvr);
    return FResultSuccess;
}

FResult TaskManager::uploadImage(const std::string& wallet, const std::shared_ptr<dbc::node_upload_image_req_data>& data) {
    if (data->image_server.empty()) {
        return FResult(ERR_ERROR, "no image_server");
    }

    rapidjson::Document doc;
    doc.Parse(data->additional.c_str());
    if (!doc.IsObject()) {
        return FResult(ERR_ERROR, "additional parse failed");
    }
    std::string image_filename;
    JSON_PARSE_STRING(doc, "image_filename", image_filename)
    if (image_filename.empty()) {
        return FResult(ERR_ERROR, "image_filename is empty");
    }

    std::string image_fullpath = image_filename;
	if (!boost::filesystem::path(image_fullpath).is_absolute())
        image_fullpath = "/data/" + image_fullpath;

    if (!boost::filesystem::exists(image_fullpath)) {
        return FResult(ERR_ERROR, "image:" + image_filename + " not exist");
    }

    if (ImageManager::instance().IsUploading(image_fullpath)) {
        return FResult(ERR_ERROR, "image:" + image_filename + " in uploading");
    }

    ImageServer imgsvr;
    imgsvr.from_string(data->image_server);
    ImageManager::instance().Upload(image_fullpath, imgsvr);
    return FResultSuccess;
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
    virDomainState vm_status = VmClient::instance().GetDomainStatus(task_id);
    if (vm_status == VIR_DOMAIN_RUNNING) return;
    if (VmClient::instance().StartDomain(task_id) == ERR_SUCCESS) {
        auto taskinfo = TaskInfoMgr::instance().getTaskInfo(task_id);
        if (taskinfo != nullptr) {
            taskinfo->status = ETaskStatus::TS_Running;
            TaskInfoMgr::instance().update(taskinfo);

            add_iptable_to_system(task_id);
        }
        TASK_LOG_INFO(task_id, "start task successful");
    }
}

void TaskManager::close_task(const std::string &task_id) {
    virDomainState vm_status = VmClient::instance().GetDomainStatus(task_id);
    if (vm_status == VIR_DOMAIN_SHUTOFF) return;
    if (VmClient::instance().DestroyDomain(task_id) == ERR_SUCCESS) {
        auto taskinfo = TaskInfoMgr::instance().getTaskInfo(task_id);
        if (taskinfo != nullptr) {
            taskinfo->status = ETaskStatus::TS_ShutOff;
            TaskInfoMgr::instance().update(taskinfo);
            remove_iptable_from_system(task_id);
        }
        TASK_LOG_INFO(taskinfo->task_id, "stop task successful");
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
        cmd = "sudo iptables -t nat -A " + chain_name + " -p tcp --destination " + public_ip + " --dport " + ssh_port +
              " --jump DNAT --to-destination " + vm_local_ip + ":22";
        run_shell(cmd);
    }
    if (!rdp_port.empty() && atoi(rdp_port) > 0) {
        cmd = "sudo iptables -t nat -A " + chain_name + " -p tcp --destination " + public_ip + " --dport " + rdp_port +
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
            cmd = "sudo iptables -t nat -A " + chain_name + " -p " + s_protocol + " --destination " + public_ip + " --dport " + s_port +
                  " --jump DNAT --to-destination " + vm_local_ip + ":" + s_port;
            run_shell(cmd);
            continue;
        }

        if (s_port.find(':') != std::string::npos) {
            std::vector<std::string> vec = util::split(s_port, ":");
            if (vec.size() == 2 && util::is_digits(vec[0]) && util::is_digits(vec[1])) {
                cmd = "sudo iptables -t nat -A " + chain_name + " -p " + s_protocol + " --destination " + public_ip + " --dport " + vec[0] +
                      " --jump DNAT --to-destination " + vm_local_ip + ":" + vec[1];
                run_shell(cmd);
                continue;
            }
        }

        if (s_port.find('-') != std::string::npos) {
            std::vector<std::string> vec = util::split(s_port, "-");
            if (vec.size() == 2 && util::is_digits(vec[0]) && util::is_digits(vec[1])) {
                cmd = "sudo iptables -t nat -A " + chain_name + " -p " + s_protocol + " --destination " + public_ip + " --dport " +
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
                    cmd = "sudo iptables -t nat -A " + chain_name + " -p " + s_protocol + " --destination " + public_ip + " --dport " +
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
    int32_t hasNvram = VmClient::instance().IsDomainHasNvram(task_id);
    if (snapshotCount > 0) undefineFlags |= VIR_DOMAIN_UNDEFINE_SNAPSHOTS_METADATA;
    if (hasNvram > 0) undefineFlags |= VIR_DOMAIN_UNDEFINE_NVRAM;

    // disk file
    std::map<std::string, domainDiskInfo> mp;
    VmClient::instance().ListDomainDiskInfo(task_id, mp);
    delete_disk_file(task_id, mp);

    // undefine task
    VmClient::instance().DestroyAndUndefineDomain(task_id, undefineFlags);

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
                if (!disk.source_file.empty() && bfs::is_regular_file(disk.source_file)) {
                    remove(disk.source_file.c_str());
                    TASK_LOG_INFO(task_id, "remove snapshot file: " << disk.source_file);
                }
            }
        }
    }
    SnapshotManager::instance().delTaskSnapshot(task_id);
    TASK_LOG_INFO(task_id, "delete task successful");
}

void TaskManager::delete_disk_file(const std::string& task_id, const std::map<std::string, domainDiskInfo> &diskfiles) {
    for (auto& iter : diskfiles) {
        if (bfs::is_regular_file(iter.second.sourceFile)) {
            remove(iter.second.sourceFile.c_str());
            TASK_LOG_INFO(task_id, "remove disk file: " << iter.second.sourceFile);
        }
    }
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

FResult TaskManager::createTask(const std::string& wallet, const std::shared_ptr<dbc::node_create_task_req_data>& data,
                                int64_t rent_end, USER_ROLE role, std::string& task_id) {
    TaskCreateParams createparams;
    FResult fret = parse_create_params(data->additional, role, createparams);
    if (fret.errcode != ERR_SUCCESS) {
        return fret;
    }

    task_id = createparams.task_id;

    std::shared_ptr<dbc::TaskInfo> taskinfo = std::make_shared<dbc::TaskInfo>();
    taskinfo->__set_task_id(createparams.task_id);
    taskinfo->__set_image_name(createparams.image_name);
    taskinfo->__set_custom_image_name(createparams.custom_image_name);
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
    taskinfo->__set_multicast(createparams.multicast);
    taskinfo->__set_network_name(createparams.network_name);
    TaskInfoMgr::instance().addTaskInfo(taskinfo);

    std::shared_ptr<TaskResource> task_resource = std::make_shared<TaskResource>();
    task_resource->cpu_sockets = createparams.cpu_sockets;
    task_resource->cpu_cores = createparams.cpu_cores; //physical cores per socket
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
    ev.image_server = data->image_server;
    add_process_task(ev);
    return FResultSuccess;
}

FResult TaskManager::parse_create_params(const std::string &additional, USER_ROLE role, TaskCreateParams& params) {
    if (additional.empty()) {
        return FResult(ERR_ERROR, "additional is empty");
    }

    rapidjson::Document doc;
    doc.Parse(additional.c_str());
    if (!doc.IsObject()) {
        return FResult(ERR_ERROR, "additional parse failed");
    }

    std::string image_name, s_ssh_port, s_rdp_port, s_gpu_count, s_cpu_cores, s_mem_size,
            s_disk_size, vm_xml, vm_xml_url, s_vnc_port, data_file_name, operation_system, bios_mode,
            custom_image_name, network_name;
    std::vector<std::string> custom_ports, multicast;

    JSON_PARSE_STRING(doc, "image_name", image_name) //image name
    JSON_PARSE_STRING(doc, "custom_image_name", custom_image_name) //custom image name
    JSON_PARSE_STRING(doc, "ssh_port", s_ssh_port)  //ssh port in linux
    JSON_PARSE_STRING(doc, "rdp_port", s_rdp_port)  //rdp port in linux
    //custom port: [
    // "tcp/udp,123",
    // "tcp/udp,111:222",
    // "tcp/udp,333-444",
    // "tcp/udp,555-666:777-888"
    //]
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
    //multicast: [
    // "223.1.1.1",
    // "224.1.1.1"
    //]
    if (doc.HasMember("multicast")) {
        const rapidjson::Value& v_multicast = doc["multicast"];
        if (v_multicast.IsArray()) {
            for (rapidjson::SizeType i = 0; i < v_multicast.Size(); i++) {
                const rapidjson::Value& v_item = v_multicast[i];
                if (v_item.IsString()) {
                    std::string str = v_item.GetString();
                    multicast.push_back(str);
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
    JSON_PARSE_STRING(doc, "network_name", network_name)
    //operation_system: "win"/"ubuntu"/""
    if (operation_system.empty()) {
        operation_system = "generic";
    }

    // rdp_port
    if (operation_system.find("win") != std::string::npos) {
        if (s_rdp_port.empty() || !util::is_digits(s_rdp_port) || atoi(s_rdp_port.c_str()) <= 0) {
            return FResult(ERR_ERROR, "rdp_port is invalid");
        }
        if (check_iptables_port_occupied(s_rdp_port)) {
            return FResult(ERR_ERROR, "rdp_port is occupied");
        }
    }
    // ssh_port
    else {
        if (s_ssh_port.empty() || !util::is_digits(s_ssh_port) || atoi(s_ssh_port.c_str()) <= 0) {
            return FResult(ERR_ERROR, "ssh_port is invalid");
        }
        if (check_iptables_port_occupied(s_ssh_port)) {
            return FResult(ERR_ERROR, "ssh_port is occupied");
        }
    }

    // vnc port range: [5900, 6000]
    if (s_vnc_port.empty()) {
        return FResult(ERR_ERROR, "vnc port is not specified");
    }
    int vnc_port = atoi(s_vnc_port.c_str());
    if (!util::is_digits(s_vnc_port) || vnc_port < 5900 || vnc_port > 6000) {
        return FResult(ERR_ERROR, "vnc_port out of range : [5900, 6000]");
    }

    //bios_mode
    if (bios_mode.empty()) {
        bios_mode = "legacy";
    }

    if (!network_name.empty()) {
        params.network_name = network_name;
        JSON_PARSE_STRING(doc, "vxlan_vni", params.vxlan_vni)
        if (!params.vxlan_vni.empty()) {
            if (!VxlanManager::instance().GetNetwork(network_name)) {
                FResult fret = VxlanManager::instance().CreateMiningNetwork(network_name, params.vxlan_vni);
                if (fret.errcode != ERR_SUCCESS) return fret;
            }
        } else {
            return FResult(ERR_ERROR, "can not find network info");
        }
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
        if (fret.errcode != ERR_SUCCESS) {
            return fret;
        }
        
        // data disk file name
        if (!data_file_name.empty()) {
            boost::filesystem::path data_path("/data/" + data_file_name);
            boost::system::error_code error_code;
            if (!boost::filesystem::exists(data_path, error_code) || error_code) {
                return FResult(ERR_ERROR, "data file does not exist");
            }

            if (!boost::filesystem::is_regular_file(data_path, error_code) || error_code) {
                return FResult(ERR_ERROR, "data file is not a regular file");
            }
        }

        // task_id
        task_id = util::create_task_id();
        if (role == USER_ROLE::UR_VERIFIER) {
            task_id = "vm_check_" + std::to_string(time(nullptr));
        }

        // cpu
        if (!util::is_digits(s_cpu_cores) || atoi(s_cpu_cores.c_str()) <= 0) {
            return FResult(ERR_ERROR, "cpu_cores is invalid");
        }

        cpu_cores = atoi(s_cpu_cores.c_str());

        if (role == USER_ROLE::UR_VERIFIER)
            cpu_cores = SystemInfo::instance().GetCpuInfo().cores;

        // gpu
        if (!util::is_digits(s_gpu_count) || atoi(s_gpu_count.c_str()) < 0) {
            return FResult(ERR_ERROR, "gpu_count is invalid");
        }

        gpu_count = atoi(s_gpu_count.c_str());

        if (role == USER_ROLE::UR_VERIFIER)
            gpu_count = SystemInfo::instance().GetGpuInfo().size();

        // mem
        if (!util::is_digits(s_mem_size) || atoi(s_mem_size.c_str()) <= 0) {
            return FResult(ERR_ERROR, "mem_size is invalid");
        }

        mem_size = atoi(s_mem_size.c_str()) * 1024L * 1024L;

        if (role == USER_ROLE::UR_VERIFIER)
            mem_size = SystemInfo::instance().GetMemInfo().free;

        // disk
        if (!util::is_digits(s_disk_size) || atoi(s_disk_size.c_str()) <= 0) {
            return FResult(ERR_ERROR, "disk_size is invalid");
        }

        uint64_t disk_size = atoi(s_disk_size.c_str()) * 1024L * 1024L;

        if (role == USER_ROLE::UR_VERIFIER)
            disk_size = (SystemInfo::instance().GetDiskInfo().available - g_disk_system_size * 1024L * 1024L) * 0.75;

        disks[1] = disk_size;

        // check
        if (!allocate_cpu(cpu_cores, sockets, cores, threads)) {
            LOG_ERROR << "allocate cpu failed";
            return FResult(ERR_ERROR, "allocate cpu failed");
        }

        if (!allocate_gpu(gpu_count, gpus)) {
            LOG_ERROR << "allocate gpu failed";
            return FResult(ERR_ERROR, "allocate gpu failed");
        }

        if (!allocate_mem(mem_size)) {
            run_shell("echo 3 > /proc/sys/vm/drop_caches");

            if (!allocate_mem(mem_size)) {
                LOG_ERROR << "allocate mem failed";
                return FResult(ERR_ERROR, "allocate mem failed");
            }
        }

        if (!allocate_disk(disk_size)) {
            LOG_ERROR << "allocate disk failed";
            return FResult(ERR_ERROR, "allocate disk failed");
        }

        // check operation system
        fret = check_operation_system(operation_system);
        if (fret.errcode != ERR_SUCCESS) {
            return fret;
        }
        // check bios mode
        fret = check_bios_mode(bios_mode);
        if (fret.errcode != ERR_SUCCESS) {
            return fret;
        }
        // check multicast
        fret = check_multicast(multicast);
        if (fret.errcode != ERR_SUCCESS) {
            return fret;
        }
    }
    else {
        if (role != USER_ROLE::UR_RENTER_WALLET && role != USER_ROLE::UR_RENTER_SESSION_ID) {
            return FResult(ERR_ERROR, "only renter can create task with vm_xml");
        }

        std::string xml_file_path;

        if (!vm_xml.empty()) {
            xml_file_path = "/data/" + vm_xml;
            boost::filesystem::path xml_path(xml_file_path);
            if (!boost::filesystem::exists(xml_path)) {
                return FResult(ERR_ERROR, xml_file_path + " not exist");
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
                return FResult(ERR_ERROR, vm_xml_url + " is invalid (please use http:// or https://)");
            }

            xml_file_path = "/data/" + util::GetFileNameFromPath(vm_xml_url);
            util::write_file(xml_file_path, xml_file_content);
            vm_xml = util::GetFileNameFromPath(xml_file_path);
        }

        ParseVmXmlParams xml_params;
        FResult fret = parse_vm_xml(xml_file_path, xml_params);
        if (fret.errcode != ERR_SUCCESS) {
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
        multicast = xml_params.multicast;

        // check image
        fret = check_image(image_name);
        if (fret.errcode != ERR_SUCCESS) {
            return fret;
        }
        fret = check_data_image(data_file_name);
        if (fret.errcode != ERR_SUCCESS) {
            return fret;
        }
        // cpu
        fret = check_cpu(sockets, cores, threads);
        if (fret.errcode != ERR_SUCCESS) {
            return fret;
        }
        // gpu
        fret = check_gpu(gpus);
        if (fret.errcode != ERR_SUCCESS) {
            return fret;
        }
        // disk
        fret = check_disk(disks);
        if (fret.errcode != ERR_SUCCESS) {
            return fret;
        }
        // check operation system
        fret = check_operation_system(operation_system);
        if (fret.errcode != ERR_SUCCESS) {
            return fret;
        }
        // check bios mode
        fret = check_bios_mode(bios_mode);
        if (fret.errcode != ERR_SUCCESS) {
            return fret;
        }
        // check multicast
        fret = check_multicast(multicast);
        if (fret.errcode != ERR_SUCCESS) {
            return fret;
        }
    }

    // params
    params.task_id = task_id;
    params.login_password = login_password;
    params.image_name = image_name;
    params.custom_image_name = custom_image_name;
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
    params.multicast = multicast;

    return FResultSuccess;
}

bool TaskManager::allocate_cpu(int32_t& total_cores, int32_t& sockets, int32_t& cores_per_socket, int32_t& threads) {
    int32_t nTotalCores = SystemInfo::instance().GetCpuInfo().cores;
    if (total_cores > nTotalCores) return false;

    int32_t nSockets = SystemInfo::instance().GetCpuInfo().physical_cpus;
    int32_t nCores = SystemInfo::instance().GetCpuInfo().physical_cores_per_cpu;
    int32_t nThreads = SystemInfo::instance().GetCpuInfo().threads_per_cpu;
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

    std::map<std::string, gpu_info> can_use_gpu = SystemInfo::instance().GetGpuInfo();
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
    return mem_size > 0 && mem_size <= SystemInfo::instance().GetMemInfo().free;
}

bool TaskManager::allocate_disk(uint64_t disk_size) {
    return disk_size > 0 && disk_size <= (SystemInfo::instance().GetDiskInfo().available - g_disk_system_size * 1024L * 1024L) * 0.75;
}

FResult TaskManager::parse_vm_xml(const std::string& xml_file_path, ParseVmXmlParams& params) {
    std::string file_content;
    boost::filesystem::load_string_file(xml_file_path, file_content);
    if (file_content.empty()) {
        return FResult(ERR_ERROR, util::GetFileNameFromPath(xml_file_path) + " file content is empty");
    }

    tinyxml2::XMLDocument doc;
    tinyxml2::XMLError err = doc.Parse(file_content.c_str());
    if (err != tinyxml2::XML_SUCCESS) {
        return FResult(ERR_ERROR, util::GetFileNameFromPath(xml_file_path) + " file(xml) parse error");
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

    // interface
    tinyxml2::XMLElement* ele_interface = el_devices->FirstChildElement("interface");
    while(ele_interface != nullptr) {
        std::string interface_type = ele_interface->Attribute("type");
        if (interface_type == "mcast") {
            tinyxml2::XMLElement* ele_interface_source = ele_interface->FirstChildElement("source");
            if (ele_interface_source != nullptr) {
                std::string mcast_address = ele_interface_source->Attribute("address");
                std::string mcast_port = ele_interface_source->Attribute("port");
                params.multicast.push_back(mcast_address + ":" + mcast_port);
            }
        }
        ele_interface = ele_interface->NextSiblingElement("interface");
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

    return FResultSuccess;
}

bool TaskManager::check_iptables_port_occupied(const std::string& port) {
    bool bFind = false;
    auto task_list = TaskInfoMgr::instance().getTasks();
    for (const auto &iter : task_list) {
        if (iter.second->ssh_port == port || iter.second->rdp_port == port) {
            bFind = true;
            break;
        }
    }
    return bFind;
}

FResult TaskManager::check_image(const std::string &image_name) {
    if (image_name.empty() || image_name.substr(image_name.size() - 5) != "qcow2") {
        return FResult(ERR_ERROR, "image_name is empty or image_name is invalid");
    }

    boost::system::error_code error_code;
    boost::filesystem::path image_path("/data/" + image_name);
    if (!boost::filesystem::exists(image_path, error_code) || error_code) {
        return FResult(ERR_ERROR, "image not exist");
    }

    if (!boost::filesystem::is_regular_file(image_path, error_code) || error_code) {
        return FResult(ERR_ERROR, "image is not a regular file");
    }

    if (ImageManager::instance().IsDownloading(image_name) ||
        ImageManager::instance().IsUploading(image_name)) {
        return FResult(ERR_ERROR, "image:" + image_name + " in downloading or uploading, please try again later");
    }

    return FResultSuccess;
}

FResult TaskManager::check_data_image(const std::string& data_image_name) {
    if (!data_image_name.empty()) {
        boost::filesystem::path image_path("/data/" + data_image_name);
        if (!boost::filesystem::exists(image_path)) {
            return FResult(ERR_ERROR, "image not exist");
        }

        if (!boost::filesystem::is_regular_file(image_path)) {
            return FResult(ERR_ERROR, "image is not a regular file");
        }
    }
    return FResultSuccess;
}

FResult TaskManager::check_cpu(int32_t sockets, int32_t cores, int32_t threads) {
    int32_t cpu_cores = sockets * cores * threads;
    if (cpu_cores <= 0 || cpu_cores > SystemInfo::instance().GetCpuInfo().cores ||
        sockets > SystemInfo::instance().GetCpuInfo().physical_cpus ||
        cores > SystemInfo::instance().GetCpuInfo().physical_cores_per_cpu ||
        threads > SystemInfo::instance().GetCpuInfo().threads_per_cpu) {
        return FResult(ERR_ERROR, "cpu config is invalid");
    }

    return FResultSuccess;
}

FResult TaskManager::check_gpu(const std::map<std::string, std::list<std::string>> &gpus) {
    std::map<std::string, gpu_info> can_use_gpu = SystemInfo::instance().GetGpuInfo();
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
            return FResult(ERR_ERROR, "gpu " + it.first + " is already in use");
        }

        for (auto& it_device : it.second) {
            std::list<std::string>& can_use_devices = it_gpu->second.devices;
            auto found = std::find(can_use_devices.begin(), can_use_devices.end(), it_device);
            if (found == can_use_devices.end()) {
                return FResult(ERR_ERROR, "gpu device " + it_device + " not found in system");
            }
        }
    }

    return FResultSuccess;
}

FResult TaskManager::check_disk(const std::map<int32_t, uint64_t> &disks) {
    uint64_t disk_available = SystemInfo::instance().GetDiskInfo().available - g_disk_system_size * 1024L * 1024L;
    uint64_t need_size = 0;
    for (auto& it : disks) {
        need_size += it.second;
    }

    if (need_size > disk_available)
        return FResult(ERR_ERROR, "no enough disk, can available size: " + std::to_string(disk_available));
    else
        return FResultSuccess;
}

FResult TaskManager::check_operation_system(const std::string& os) {
    if (os.empty()) return FResultSuccess;
    if (os == "generic") return FResultSuccess;
    if (os.find("ubuntu") != std::string::npos) return FResultSuccess;
    if (os.find("win") != std::string::npos) return FResultSuccess;
    return FResult(ERR_ERROR, "unsupported operation system");
}

FResult TaskManager::check_bios_mode(const std::string& bios_mode) {
    if (bios_mode.empty()) return FResultSuccess;
    if (bios_mode == "legacy") return FResultSuccess;
    if (bios_mode == "uefi") return FResultSuccess;
    return FResult(ERR_ERROR, "bios mode only supported [legacy] or [uefi]");
}

FResult TaskManager::check_multicast(const std::vector<std::string>& multicast) {
    if (multicast.empty()) return FResultSuccess;

    for (const auto& address : multicast) {
        std::vector<std::string> vecSplit = util::split(address, ":");
        if (vecSplit.size() != 2) {
            return FResult(ERR_ERROR, "multicast format: ip:port");
        }
        boost::asio::ip::tcp::endpoint ep(boost::asio::ip::address_v4::from_string(vecSplit[0]), atoi(vecSplit[1]));
        if (!ep.address().is_multicast()) {
            return FResult(ERR_ERROR, "address is not a multicast address");
        }
    }
    return FResultSuccess;
}

FResult TaskManager::parse_create_snapshot_params(const std::string &additional, const std::string &task_id,
                                                  std::shared_ptr<dbc::snapshotInfo> &info) {
    if (additional.empty()) {
        return FResult(ERR_ERROR, "additional is empty");
    }

    rapidjson::Document doc;
    doc.Parse(additional.c_str());
    if (!doc.IsObject()) {
        return FResult(ERR_ERROR, "additional(json) parse failed");
    }

    std::string snapshot_name, description;
    JSON_PARSE_STRING(doc, "snapshot_name", snapshot_name)
    JSON_PARSE_STRING(doc, "description", description)

    if (snapshot_name.empty()) {
        return FResult(ERR_ERROR, "snapshot_name is not specified");
    }

    if (SnapshotManager::instance().getTaskSnapshot(task_id, snapshot_name, false) != nullptr) {
        return FResult(ERR_ERROR, "snapshot_name is already exist");
    }

    if (description.empty()) {
        return FResult(ERR_ERROR, "snapshot description is not specified");
    }

    info->__set_name(snapshot_name);
    info->__set_description(description);

    std::map<std::string, domainDiskInfo> ddInfos;
    if (!VmClient::instance().ListDomainDiskInfo(task_id, ddInfos) || ddInfos.empty()) {
        return FResult(ERR_ERROR, "can not get vm disk info");
    }
    for (const auto & disk : ddInfos) {
        LOG_INFO << "list domain disk name:" << disk.second.targetDev << ", driver name:" << disk.second.driverName
        << ", driver type:" << disk.second.driverType << ", source file:" << disk.second.sourceFile << ", target bus:" << disk.second.targetBus;
    }

    std::vector<dbc::snapshotDiskInfo> disks;
    if(doc.HasMember("disks")) {
        const rapidjson::Value& obj_disks = doc["disks"];
        if (!obj_disks.IsArray()) {
            return FResult(ERR_ERROR, "additional disks must be an array");
        }
        for (size_t i = 0; i < obj_disks.Size(); i++) {
            const rapidjson::Value& v = obj_disks[i];
            if (!v.IsObject()) {
                return FResult(ERR_ERROR, "parse additional disks item json failed");
            }
            dbc::snapshotDiskInfo disk;
            if (!v.HasMember("disk_name") || !v["disk_name"].IsString()) {
                return FResult(ERR_ERROR, "parse additional disk_name json failed");
            }
            std::string disk_name = v["disk_name"].GetString();
            disk.__set_name(v["disk_name"].GetString());
            if (ddInfos.find(disk.name) == ddInfos.end()) {
                return FResult(ERR_ERROR, "additional disk_name not exist in task:" + task_id);
            }
            if (!v.HasMember("snapshot_type")) {
                disk.__set_snapshot("external");
            } else {
                if (!v["snapshot_type"].IsString()) {
                    return FResult(ERR_ERROR, "parse additional disks item snapshot_type json failed");
                }
                disk.__set_snapshot(v["snapshot_type"].GetString());
                if (disk.snapshot.empty()) {
                    return FResult(ERR_ERROR, "additional disks item snapshot_type can not empty");
                }
                if (!(disk.snapshot == "external" || disk.snapshot == "no" || disk.snapshot == "internal")) {
                    return FResult(ERR_ERROR, "invalid additional disks item snapshot_type value:" + disk.snapshot);
                }
            }
            if (!v.HasMember("driver")) {
                disk.__set_driver_type("qcow2");
            } else {
                if (!v["driver"].IsString()) {
                    return FResult(ERR_ERROR, "parse additional disks item driver json failed");
                }
                disk.__set_driver_type(v["driver"].GetString());
                if (disk.driver_type.empty()) {
                    return FResult(ERR_ERROR, "additional disks item driver can not empty");
                }
                if (disk.driver_type != "qcow2") {
                    return FResult(ERR_ERROR, "additional disks item driver only support qcow2");
                }
            }
            if (v.HasMember("snapshot_file_name")) {
                if (!v["snapshot_file_name"].IsString()) {
                    return FResult(ERR_ERROR, "parse additional disks item snapshot_file_name json failed");
                }
                std::string snapshot_file_name = v["snapshot_file_name"].GetString();
                if (snapshot_file_name.find("/") != std::string::npos || snapshot_file_name.find(".qcow2") == std::string::npos) {
                    return FResult(ERR_ERROR, "invalid snapshot_file_name, use format: xxx.qcow2");
                }
                std::string snapshot_file_path = "/data/" + snapshot_file_name;
                boost::filesystem::path disk_source_file(snapshot_file_path);
                if (disk_source_file.extension().string() != ".qcow2") {
                    return FResult(ERR_ERROR, "disk snapshot_file_name must end with .qcow2, such as xxx.qcow2");
                }
                boost::system::error_code err;
                if (boost::filesystem::exists(disk_source_file, err)) {
                    return FResult(ERR_ERROR, "disks item snapshot_file_name already exist");
                }
                // if (err) {
                //     return FResult(ERR_ERROR, "check disk item snapshot_file_name error:" + err.message());
                // }
                disk.__set_source_file(snapshot_file_path);
            } else {
                std::string snap_file = "snap_" + std::to_string(rand() % 100000) + "_" + util::time2str(time(nullptr)) +
                        "_" + snapshot_name + "-" + disk_name + ".qcow2";
                disk.__set_source_file("/data/" + snap_file);
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

            std::string snap_file = "snap_" + std::to_string(rand() % 100000) + "_" + util::time2str(time(nullptr)) +
                    "_" + snapshot_name + "-" + sdInfo.name + ".qcow2";
            sdInfo.__set_source_file("/data/" + snap_file);
            disks.push_back(sdInfo);
        }
    }

    if (disks.empty()) {
        return FResult(ERR_ERROR, "disks is empty");
    }
    info->__set_disks(disks);
    for (auto& disk : info->disks) {
        LOG_INFO << "parse additional disk name:" << disk.name << ", driver type:" << disk.driver_type
                 << ", snapshot type:" << disk.snapshot << ", source file:" << disk.source_file;
        if (disk.snapshot == "external" && disk.source_file.empty()) {
            std::string snap_file = "snap_" + std::to_string(rand() % 100000) + "_" + util::time2str(time(nullptr)) +
                    "_" + snapshot_name + "-" + disk.name + ".qcow2";
            disk.source_file = "/data/" + snap_file;
        }
    }

    return FResultSuccess;
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
        return FResult(ERR_ERROR, "task_id not exist");
    }

    if (!VmClient::instance().IsExistDomain(task_id)) {
        return FResult(ERR_ERROR, "domain not exist");
    }

    //TODO： check task resource


    virDomainState vm_status = VmClient::instance().GetDomainStatus(task_id);
    if (vm_status == VIR_DOMAIN_NOSTATE || vm_status == VIR_DOMAIN_SHUTOFF || vm_status == VIR_DOMAIN_PMSUSPENDED) {
        taskinfo->__set_status(TS_Starting);
        taskinfo->__set_last_start_time(time(nullptr));
        TaskInfoMgr::instance().update(taskinfo);

        ETaskEvent ev;
        ev.task_id = task_id;
        ev.op = T_OP_Start;
        add_process_task(ev);
        return FResultSuccess;
    } else {
        return FResult(ERR_ERROR, "task is " + vm_status_string(vm_status));
    }
}

FResult TaskManager::stopTask(const std::string& wallet, const std::string &task_id) {
    auto taskinfo = TaskInfoMgr::instance().getTaskInfo(task_id);
    if (taskinfo == nullptr) {
        return FResult(ERR_ERROR, "task_id not exist");
    }

    if (!VmClient::instance().IsExistDomain(task_id)) {
        return FResult(ERR_ERROR, "domain not exist");
    }

    virDomainState vm_status = VmClient::instance().GetDomainStatus(task_id);
    if (vm_status != VIR_DOMAIN_SHUTOFF) {
        taskinfo->__set_status(TS_Stopping);
        taskinfo->__set_last_stop_time(time(nullptr));
        TaskInfoMgr::instance().update(taskinfo);

        ETaskEvent ev;
        ev.task_id = task_id;
        ev.op = T_OP_Stop;
        add_process_task(ev);
        return FResultSuccess;
    } else {
        return FResult(ERR_ERROR, "task is " + vm_status_string(vm_status));
    }
}

FResult TaskManager::restartTask(const std::string& wallet, const std::string &task_id, bool force_reboot) {
    auto taskinfo = TaskInfoMgr::instance().getTaskInfo(task_id);
    if (taskinfo == nullptr) {
        return FResult(ERR_ERROR, "task_id not exist");
    }

    if (!VmClient::instance().IsExistDomain(task_id)) {
        return FResult(ERR_ERROR, "domain not exist");
    }

    // TODO: check task resource


    virDomainState vm_status = VmClient::instance().GetDomainStatus(task_id);
    if (vm_status == VIR_DOMAIN_NOSTATE || vm_status == VIR_DOMAIN_SHUTOFF || vm_status == VIR_DOMAIN_RUNNING) {
        taskinfo->__set_status(TS_Restarting);
        taskinfo->__set_last_start_time(time(nullptr));
        TaskInfoMgr::instance().update(taskinfo);

        ETaskEvent ev;
        ev.task_id = task_id;
        ev.op = force_reboot ? T_OP_ForceReboot : T_OP_ReStart;
        add_process_task(ev);
        return FResultSuccess;
    } else {
        return FResult(ERR_ERROR, "task is " + vm_status_string(vm_status));
    }
}

FResult TaskManager::resetTask(const std::string& wallet, const std::string &task_id) {
    return FResultSuccess;

    auto taskinfo = TaskInfoMgr::instance().getTaskInfo(task_id);
    if (taskinfo == nullptr) {
        return FResult(ERR_ERROR, "task_id not exist");
    }

    if (!VmClient::instance().IsExistDomain(task_id)) {
        return FResult(ERR_ERROR, "domain not exist");
    }

    virDomainState vm_status = VmClient::instance().GetDomainStatus(task_id);
    if (vm_status == VIR_DOMAIN_RUNNING) {
        taskinfo->__set_status(TS_Resetting);
        taskinfo->__set_last_start_time(time(nullptr));
        TaskInfoMgr::instance().update(taskinfo);

        ETaskEvent ev;
        ev.task_id = task_id;
        ev.op = T_OP_Reset;
        add_process_task(ev);
        return FResultSuccess;
    } else {
        return FResult(ERR_ERROR, "task is " + vm_status_string(vm_status));
    }
}

FResult TaskManager::deleteTask(const std::string& wallet, const std::string &task_id) {
    auto taskinfo = TaskInfoMgr::instance().getTaskInfo(task_id);
    if (taskinfo == nullptr) {
        return FResult(ERR_ERROR, "task_id not exist");
    }

    LOG_INFO << "deleteTask: wallet=" << wallet << ", task_id=" << task_id;

    virDomainState vm_status = VmClient::instance().GetDomainStatus(task_id);
    taskinfo->__set_status(TS_Deleting);
    taskinfo->__set_last_stop_time(time(nullptr));
    TaskInfoMgr::instance().update(taskinfo);

    ETaskEvent ev;
    ev.task_id = task_id;
    ev.op = T_OP_Delete;
    add_process_task(ev);
    return FResultSuccess;
}

FResult TaskManager::modifyTask(const std::string& wallet, const std::shared_ptr<dbc::node_modify_task_req_data>& data) {
    std::string task_id = data->task_id;
    auto taskinfoPtr = TaskInfoManager::instance().getTaskInfo(task_id);
    if (taskinfoPtr == nullptr) {
        return FResult(ERR_ERROR, "task:" + task_id + " not exist");
    }

	if (!VmClient::instance().IsExistDomain(task_id)) {
		return FResult(ERR_ERROR, "task:" + task_id + " not exist");
	}

    virDomainState vm_status = VmClient::instance().GetDomainStatus(task_id);
    if (vm_status != VIR_DOMAIN_SHUTOFF) {
        return FResult(ERR_ERROR, "task is running, please stop it");
    }

    rapidjson::Document doc;
    doc.Parse(data->additional.c_str());
    if (!doc.IsObject()) {
        return FResult(ERR_ERROR, "additional parse failed");
    }

    std::string old_ssh_port, new_ssh_port;
    JSON_PARSE_STRING(doc, "new_ssh_port", new_ssh_port);
    if (!new_ssh_port.empty()) {
        int32_t nport = atoi(new_ssh_port.c_str());
        if (nport <= 0) {
            return FResult(ERR_ERROR, "new_ssh_port " + new_ssh_port + " is invalid! (usage: > 0)");
        }
        bool bFound = false;
        auto task_list = TaskInfoMgr::instance().getTasks();
        for (const auto &iter : task_list) {
            if (iter.first != task_id) {
                if (iter.second->ssh_port == new_ssh_port || iter.second->rdp_port == new_ssh_port) {
                    bFound = true;
                    break;
                }
            }
        }
        if (bFound) {
            return FResult(ERR_ERROR, "new_ssh_port " + new_ssh_port + " has been used!");
        }

        old_ssh_port = taskinfoPtr->ssh_port;
        taskinfoPtr->ssh_port = new_ssh_port;
        TaskInfoManager::instance().update(taskinfoPtr);

        auto taskIptablePtr = TaskIptableManager::instance().getIptable(task_id);
        if (taskIptablePtr != nullptr) {
            taskIptablePtr->ssh_port = new_ssh_port;
            TaskIptableManager::instance().update(taskIptablePtr);
        }
    }

    std::string new_rdp_port;
    JSON_PARSE_STRING(doc, "new_rdp_port", new_rdp_port);
    if (!new_rdp_port.empty()) {
        if (atoi(new_rdp_port.c_str()) <= 0) {
            return FResult(ERR_ERROR, "new_rdp_port " + new_rdp_port + " is invalid! (usage: > 0)");
        }
        bool bFound = false;
        auto task_list = TaskInfoMgr::instance().getTasks();
        for (const auto &iter : task_list) {
            if (iter.first != task_id) {
                if (iter.second->ssh_port == new_rdp_port || iter.second->rdp_port == new_rdp_port) {
                    bFound = true;
                    break;
                }
            }
        }
        if (bFound) {
            return FResult(ERR_ERROR, "new_rdp_port " + new_rdp_port + " has been used!");
        }

        taskinfoPtr->rdp_port = new_rdp_port;
        TaskInfoManager::instance().update(taskinfoPtr);

        auto taskIptablePtr = TaskIptableManager::instance().getIptable(task_id);
        if (taskIptablePtr != nullptr) {
            taskIptablePtr->rdp_port = new_rdp_port;
            TaskIptableManager::instance().update(taskIptablePtr);
        }
    }

    if (!new_ssh_port.empty() && !new_rdp_port.empty()
        && new_ssh_port == new_rdp_port) {
        return FResult(ERR_ERROR, "new_ssh_port and new_rdp_port are the same!");
    }

    std::vector<std::string> new_custom_port;
    if (doc.HasMember("new_custom_port")) {
        const rapidjson::Value& v_custom_port = doc["new_custom_port"];
        if (v_custom_port.IsArray()) {
            for (rapidjson::SizeType i = 0; i < v_custom_port.Size(); i++) {
                new_custom_port.push_back(v_custom_port[i].GetString());
            }

            if (!new_custom_port.empty()) {
                taskinfoPtr->custom_port = new_custom_port;
                TaskInfoManager::instance().update(taskinfoPtr);

                auto taskIptablePtr = TaskIptableManager::instance().getIptable(task_id);
                if (taskIptablePtr != nullptr) {
                    taskIptablePtr->custom_port = new_custom_port;
                    TaskIptableManager::instance().update(taskIptablePtr);
                }
            }
        }
    }

	auto taskIptablePtr = TaskIptableManager::instance().getIptable(task_id);
    if (taskIptablePtr != nullptr) {
        shell_remove_iptable_from_system(task_id, taskIptablePtr->host_ip, old_ssh_port, taskIptablePtr->vm_local_ip);
        //shell_add_iptable_to_system(task_id, taskIptablePtr->host_ip, new_ssh_port, new_rdp_port, new_custom_port, taskIptablePtr->vm_local_ip);
    }

	std::string new_vnc_port;
	JSON_PARSE_STRING(doc, "new_vnc_port", new_vnc_port);
	if (!new_vnc_port.empty()) {
        int32_t nport = atoi(new_vnc_port.c_str());
        if (nport < 5900 || nport > 6000) {
            return FResult(ERR_ERROR, "new_vnc_port " + new_vnc_port + " is invalid! (usage: 5900 =< port <= 6000)");
        }

		auto taskResourcePtr = TaskResourceManager::instance().getTaskResource(task_id);
		if (taskResourcePtr != nullptr) {
			taskResourcePtr->vnc_port = atoi(new_vnc_port);
		}
	}

    std::string new_gpu_count;
    std::map<std::string, std::list<std::string>> new_gpus;
    JSON_PARSE_STRING(doc, "new_gpu_count", new_gpu_count);
    if (!new_gpu_count.empty()) {
		auto taskResourcePtr = TaskResourceManager::instance().getTaskResource(task_id);
        if (taskResourcePtr != nullptr) {
            int count = atoi(new_gpu_count.c_str());
            if (allocate_gpu(count, new_gpus)) {
                taskResourcePtr->gpus = new_gpus;
            }
            else {
                return FResult(ERR_ERROR, "allocate gpu failed");
            }
        }
    }

    std::string new_cpu_cores;
    JSON_PARSE_STRING(doc, "new_cpu_cores", new_cpu_cores);
    if (!new_cpu_cores.empty()) {
		auto taskResourcePtr = TaskResourceManager::instance().getTaskResource(task_id);
        if (taskResourcePtr != nullptr) {
            int count = atoi(new_cpu_cores);
            int32_t cpu_sockets = 1, cpu_cores = 1, cpu_threads = 2;
            if (allocate_cpu(count, cpu_sockets, cpu_cores, cpu_threads)) {
                taskResourcePtr->cpu_sockets = cpu_sockets;
                taskResourcePtr->cpu_cores = cpu_cores;
                taskResourcePtr->cpu_threads = cpu_threads;
            }
            else {
                return FResult(ERR_ERROR, "allocate cpu failed");
            }
        }
    }

    std::string new_mem_size;
    JSON_PARSE_STRING(doc, "new_mem_size", new_mem_size);
    if (!new_mem_size.empty()) {
		auto taskResourcePtr = TaskResourceManager::instance().getTaskResource(task_id);
        if (taskResourcePtr != nullptr) {
            int64_t ksize = atoi(new_mem_size.c_str()) * 1024L * 1024L;
            if (allocate_mem(ksize)) {
                taskResourcePtr->mem_size = ksize;
            }
            else {
                run_shell("echo 3 > /proc/sys/vm/drop_caches");

                if (allocate_mem(ksize)) {
                    taskResourcePtr->mem_size = ksize;
                } else {
                    return FResult(ERR_ERROR, "allocate memory failed");
                }
            }
        }
    }

    std::string increase_disk_size;
    bool increase_disk = false;
    JSON_PARSE_STRING(doc, "increase_disk_size", increase_disk_size);
    if (!increase_disk_size.empty()) {
		auto taskResourcePtr = TaskResourceManager::instance().getTaskResource(task_id);
        if (taskResourcePtr != nullptr) {
            int64_t ksize = atoi(increase_disk_size.c_str()) * 1024L * 1024L;
            if (ksize > 0) {
                if (allocate_disk(ksize)) {
                    int idx = taskResourcePtr->disks.size() + 1;
                    taskResourcePtr->disks[idx] = ksize;
                    increase_disk = true;
                }
                else {
                    return FResult(ERR_ERROR, "allocate disk failed");
                }
            }
        }
    }

	auto taskResourcePtr = TaskResourceManager::instance().getTaskResource(task_id);
    if (taskResourcePtr != nullptr) {
        FResult ret = VmClient::instance().RedefineDomain(taskinfoPtr, taskResourcePtr, increase_disk);
        if (ret.errcode != ERR_SUCCESS) {
            return ret;
        }
    }

    return FResultSuccess;
}

FResult TaskManager::getTaskLog(const std::string &task_id, ETaskLogDirection direction, int32_t nlines,
                                std::string &log_content) {
    auto taskinfo = TaskInfoMgr::instance().getTaskInfo(task_id);
    if (taskinfo == nullptr) {
        log_content = "";
        return FResult(ERR_ERROR, "task_id not exist");
    } else {
        return VmClient::instance().GetDomainLog(task_id, direction, nlines, log_content);
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
        virDomainState vm_status = VmClient::instance().GetDomainStatus(task_id);
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
        } else if (ts == TS_Running && vm_status == VIR_DOMAIN_PMSUSPENDED) {
            return TS_PMSuspended;
        }

        return ts;
    }
}

int32_t TaskManager::getTaskAgentInterfaceAddress(const std::string &task_id, std::vector<std::tuple<std::string, std::string>> &address) {
    virDomainState vm_status = VmClient::instance().GetDomainStatus(task_id);
    if (vm_status != VIR_DOMAIN_RUNNING) return -1;
    std::vector<dbc::virDomainInterface> difaces;
    if (VmClient::instance().GetDomainInterfaceAddress(task_id, difaces, VIR_DOMAIN_INTERFACE_ADDRESSES_SRC_AGENT) > 0) {
        for (const auto& diface : difaces) {
            for (const auto& addr : diface.addrs) {
                if (addr.type == 0 && strncmp(addr.addr.c_str(), "127.", 4) != 0 && strncmp(addr.addr.c_str(), "192.168.122.", 12) != 0) {
                    // LOG_INFO << diface.name << " " << diface.hwaddr << " " << addr.type << " " << addr.addr << " " << addr.prefix;
                    address.push_back(std::make_tuple(diface.hwaddr, addr.addr));
                }
            }
        }
    }
    return address.size();
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
        TASK_LOG_INFO(task_id, "delete check task");
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
        TASK_LOG_INFO(task_id, "delete other check task");
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
        VmClient::instance().ListDomainDiskInfo(task_id, disks);
    }
}

FResult TaskManager::createSnapshot(const std::string& wallet, const std::string &additional, const std::string& task_id) {
    auto taskinfo = TaskInfoMgr::instance().getTaskInfo(task_id);
    if (taskinfo == nullptr) {
        return FResult(ERR_ERROR, "task_id not exist");
    }

    if (!VmClient::instance().IsExistDomain(task_id)) {
        return FResult(ERR_ERROR, "domain not exist");
    }

    {
        std::shared_ptr<dbc::snapshotInfo> temp = SnapshotManager::instance().getCreatingSnapshot(task_id);
        if (temp != nullptr && !temp->__isset.error_code) {
            return FResult(ERR_ERROR, "task snapshot is busy, please wait a moment");
        }
    }
    std::shared_ptr<dbc::snapshotInfo> sInfo = std::make_shared<dbc::snapshotInfo>();
    FResult fret = parse_create_snapshot_params(additional, task_id, sInfo);
    if (fret.errcode != ERR_SUCCESS) {
        return fret;
    }

    //TODO： check task resource
    SnapshotManager::instance().addCreatingSnapshot(task_id, sInfo);

    virDomainState vm_status = VmClient::instance().GetDomainStatus(task_id);
    if (/*vm_status == VIR_DOMAIN_RUNNING || */vm_status == VIR_DOMAIN_SHUTOFF) {
        taskinfo->__set_status(TS_CreatingSnapshot);
        taskinfo->__set_last_stop_time(time(nullptr));
        TaskInfoMgr::instance().update(taskinfo);

        ETaskEvent ev;
        ev.task_id = task_id;
        ev.op = T_OP_CreateSnapshot;
        add_process_task(ev);
        return FResultSuccess;
    } else {
        return FResult(ERR_ERROR, "task is " + vm_status_string(vm_status) + 
            ", please save the job and shutdown task before creating a snapshot");
    }
}

FResult TaskManager::deleteSnapshot(const std::string& wallet, const std::string &task_id, const std::string& snapshot_name) {
    return FResult(ERR_ERROR, "The function is not implemented yet.");
}

FResult TaskManager::listTaskSnapshot(const std::string& wallet, const std::string& task_id, std::vector<std::shared_ptr<dbc::snapshotInfo>>& snaps) {
    // auto renttask = WalletRentTaskMgr::instance().getRentTask(wallet);
    // if (renttask == nullptr) {
    //     return FResult(ERR_ERROR, "this wallet has none rent task");
    // }
    // std::shared_ptr<dbc::TaskInfo> taskinfo = nullptr;
    // for (auto& id : renttask->task_ids) {
    //     if (id == task_id) {
    //         taskinfo = TaskInfoMgr::instance().getTaskInfo(task_id);
    //         break;
    //     }
    // }
    // if (taskinfo == nullptr) {
    //     return FResult(ERR_ERROR, "task_id not exist");
    // }
    SnapshotManager::instance().listTaskSnapshot(task_id, snaps);
    return FResultSuccess;
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
    int32_t res_code = ERR_ERROR;
    std::string res_msg;
    switch (ev.op) {
        case T_OP_Create:
            process_create(ev);
            break;
        case T_OP_Start:
            process_start(ev);
            break;
        case T_OP_Stop:
            process_stop(ev);
            break;
        case T_OP_ReStart:
            process_restart(ev);
            break;
        case T_OP_ForceReboot:
            process_force_reboot(ev);
            break;
        case T_OP_Reset:
            process_reset(ev);
            break;
        case T_OP_Delete:
            process_delete(ev);
            break;
        case T_OP_CreateSnapshot:
            process_create_snapshot(ev);
            break;
        default:
            TASK_LOG_ERROR(ev.task_id, "unknown op:" << ev.op);
            break;
    }
}

void TaskManager::process_create(const ETaskEvent& ev) {
    auto taskinfo = TaskInfoMgr::instance().getTaskInfo(ev.task_id);
    if (taskinfo == nullptr) return;

    auto task_resource = TaskResourceMgr::instance().getTaskResource(taskinfo->task_id);
    if (task_resource == nullptr) {
        taskinfo->status = TS_CreateError;
        TaskInfoMgr::instance().update(taskinfo);
        TASK_LOG_ERROR(taskinfo->task_id, "not found task resource");
        return;
    }

    ImageServer imgsvr;
    imgsvr.from_string(ev.image_server);
    std::vector<std::string> images_name;
    getNeededBackingImage(taskinfo->image_name, images_name);
    if (!images_name.empty()) {
        if (ev.image_server.empty()) {
            taskinfo->status = TS_CreateError;
            TaskInfoMgr::instance().update(taskinfo);
            TASK_LOG_ERROR(taskinfo->task_id, "image not exist and no image_server");
            return;
        }

        std::string _taskid = ev.task_id;
        std::string _svr = ev.image_server;
        ImageServer imgsvr;
        imgsvr.from_string(_svr);
        LOG_INFO << "need download image: " << images_name[0];
        ImageManager::instance().Download(images_name[0], "/data/", imgsvr, [this, _taskid, _svr]() {
            ETaskEvent ev;
            ev.task_id = _taskid;
            ev.op = T_OP_Create;
            ev.image_server = _svr;
            add_process_task(ev);
        });
        return;
    } else {
        if (ImageManager::instance().IsDownloading(taskinfo->image_name) ||
            ImageManager::instance().IsUploading(taskinfo->image_name)) {
            taskinfo->status = TS_CreateError;
            TaskInfoMgr::instance().update(taskinfo);
            TASK_LOG_ERROR(taskinfo->task_id, "image:" << taskinfo->image_name
                            << " in downloading or uploading, please try again later");
            return;
        }
    }

    ERRCODE ret = VmClient::instance().CreateDomain(taskinfo, task_resource);
    if (ret != ERR_SUCCESS) {
        taskinfo->status = TS_CreateError;
        TaskInfoMgr::instance().update(taskinfo);
        TASK_LOG_ERROR(taskinfo->task_id, "create domain failed");
    } else {
        std::string local_ip = VmClient::instance().GetDomainLocalIP(taskinfo->task_id);
        if (!local_ip.empty()) {
            if (!VmClient::instance().SetDomainUserPassword(taskinfo->task_id,
                    taskinfo->operation_system.find("win") == std::string::npos ? g_vm_ubuntu_login_username : g_vm_windows_login_username,
                    taskinfo->login_password)) {
                VmClient::instance().DestroyDomain(taskinfo->task_id);

                taskinfo->status = TS_CreateError;
                TaskInfoMgr::instance().update(taskinfo);
                TASK_LOG_ERROR(taskinfo->task_id, "set domain password failed");
                return;
            }

            if (!create_task_iptable(taskinfo->task_id, taskinfo->ssh_port, taskinfo->rdp_port,
                                     taskinfo->custom_port, local_ip)) {
                VmClient::instance().DestroyDomain(taskinfo->task_id);

                taskinfo->status = TS_CreateError;
                TaskInfoMgr::instance().update(taskinfo);
                TASK_LOG_ERROR(taskinfo->task_id, "create task iptable failed");
                return;
            }

            taskinfo->status = TS_Running;
            TaskInfoMgr::instance().update(taskinfo);
            TASK_LOG_INFO(taskinfo->task_id, "create task successful");
        } else {
            VmClient::instance().DestroyDomain(taskinfo->task_id);

            taskinfo->status = TS_CreateError;
            TaskInfoMgr::instance().update(taskinfo);
            TASK_LOG_ERROR(taskinfo->task_id, "get domain local ip failed");
        }
    }
}

bool TaskManager::create_task_iptable(const std::string &domain_name, const std::string &ssh_port,
                                      const std::string& rdp_port, const std::vector<std::string>& custom_port,
                                      const std::string &vm_local_ip) {
    // std::string public_ip = SystemInfo::instance().publicip();
    std::string public_ip = SystemInfo::instance().GetDefaultRouteIp();
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

void TaskManager::process_start(const ETaskEvent& ev) {
    auto taskinfo = TaskInfoMgr::instance().getTaskInfo(ev.task_id);
    if (taskinfo == nullptr) return;

    ERRCODE ret = VmClient::instance().StartDomain(taskinfo->task_id);
    if (ret != ERR_SUCCESS) {
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

void TaskManager::process_stop(const ETaskEvent& ev) {
    auto taskinfo = TaskInfoMgr::instance().getTaskInfo(ev.task_id);
    if (taskinfo == nullptr) return;

    ERRCODE ret = VmClient::instance().DestroyDomain(taskinfo->task_id);
    if (ret != ERR_SUCCESS) {
        taskinfo->status = TS_StopError;
        TaskInfoMgr::instance().update(taskinfo);
        TASK_LOG_ERROR(taskinfo->task_id, "stop task failed");
    } else {
        taskinfo->status = TS_ShutOff;
        TaskInfoMgr::instance().update(taskinfo);
        remove_iptable_from_system(taskinfo->task_id);
        TASK_LOG_INFO(taskinfo->task_id, "stop task successful");
    }
}

void TaskManager::process_restart(const ETaskEvent& ev) {
    auto taskinfo = TaskInfoMgr::instance().getTaskInfo(ev.task_id);
    if (taskinfo == nullptr) return;

    virDomainState vm_status = VmClient::instance().GetDomainStatus(taskinfo->task_id);
    if (vm_status == VIR_DOMAIN_SHUTOFF) {
        ERRCODE ret = VmClient::instance().StartDomain(taskinfo->task_id);
        if (ret != ERR_SUCCESS) {
            taskinfo->status = TS_RestartError;
            TaskInfoMgr::instance().update(taskinfo);
            TASK_LOG_ERROR(taskinfo->task_id, "restart task failed");
        } else {
            taskinfo->status = TS_Running;
            TaskInfoMgr::instance().update(taskinfo);
            add_iptable_to_system(taskinfo->task_id);
            TASK_LOG_INFO(taskinfo->task_id, "restart task successful");
        }
    } else if (vm_status == VIR_DOMAIN_RUNNING) {
        ERRCODE ret = VmClient::instance().RebootDomain(taskinfo->task_id);
        if (ret != ERR_SUCCESS) {
            taskinfo->status = TS_RestartError;
            TaskInfoMgr::instance().update(taskinfo);
            TASK_LOG_ERROR(taskinfo->task_id, "restart task failed");
        } else {
            taskinfo->status = TS_Running;
            TaskInfoMgr::instance().update(taskinfo);
            add_iptable_to_system(taskinfo->task_id);
            TASK_LOG_INFO(taskinfo->task_id, "restart task successful");
        }
    }
}

void TaskManager::process_force_reboot(const ETaskEvent& ev) {
    auto taskinfo = TaskInfoMgr::instance().getTaskInfo(ev.task_id);
    if (taskinfo == nullptr) return;

    virDomainState vm_status = VmClient::instance().GetDomainStatus(taskinfo->task_id);
    if (vm_status == VIR_DOMAIN_SHUTOFF) {
        ERRCODE ret = VmClient::instance().StartDomain(taskinfo->task_id);
        if (ret != ERR_SUCCESS) {
            taskinfo->status = TS_RestartError;
            TaskInfoMgr::instance().update(taskinfo);
            TASK_LOG_ERROR(taskinfo->task_id, "restart task failed");
        } else {
            taskinfo->status = TS_Running;
            TaskInfoMgr::instance().update(taskinfo);
            add_iptable_to_system(taskinfo->task_id);
            TASK_LOG_INFO(taskinfo->task_id, "restart task successful");
        }
    } else /*if (vm_status == VIR_DOMAIN_RUNNING)*/ {
        ERRCODE ret = VmClient::instance().DestroyDomain(taskinfo->task_id);
        if (ret != ERR_SUCCESS) {
            taskinfo->status = TS_RestartError;
            TaskInfoMgr::instance().update(taskinfo);
            TASK_LOG_ERROR(taskinfo->task_id, "restart task failed");
        } else {
            sleep(3);
            if ((ret = VmClient::instance().StartDomain(taskinfo->task_id)) == ERR_SUCCESS) {
                taskinfo->status = TS_Running;
                TaskInfoMgr::instance().update(taskinfo);
                add_iptable_to_system(taskinfo->task_id);
                TASK_LOG_INFO(taskinfo->task_id, "restart task successful");
            } else {
                taskinfo->status = TS_RestartError;
                TaskInfoMgr::instance().update(taskinfo);
                TASK_LOG_ERROR(taskinfo->task_id, "restart task failed");
            }
        }
    }
}

void TaskManager::process_reset(const ETaskEvent& ev) {
    auto taskinfo = TaskInfoMgr::instance().getTaskInfo(ev.task_id);
    if (taskinfo == nullptr) return;

    ERRCODE ret = VmClient::instance().ResetDomain(taskinfo->task_id);
    if (ret != ERR_SUCCESS) {
        taskinfo->status = TS_ResetError;
        TaskInfoMgr::instance().update(taskinfo);
        TASK_LOG_ERROR(taskinfo->task_id, "reset task failed");
    } else {
        taskinfo->status = TS_Running;
        TaskInfoMgr::instance().update(taskinfo);
        TASK_LOG_INFO(taskinfo->task_id, "reset task successful");
    }
}

void TaskManager::process_delete(const ETaskEvent& ev) {
    auto taskinfo = TaskInfoMgr::instance().getTaskInfo(ev.task_id);
    if (taskinfo == nullptr) return;

    LOG_INFO << "process_delete: task_id=" << ev.task_id;

    delete_task(taskinfo->task_id);
}

void TaskManager::process_create_snapshot(const ETaskEvent& ev) {
    auto taskinfo = TaskInfoMgr::instance().getTaskInfo(ev.task_id);
    if (taskinfo == nullptr) return;

    std::shared_ptr<dbc::snapshotInfo> info = SnapshotManager::instance().getCreatingSnapshot(taskinfo->task_id);
    if (info == nullptr) {
        LOG_ERROR << "can not find snapshot:" << info->name << " info when creating a snapshot on vm:" << taskinfo->task_id;
        return;
    }
    FResult result = VmClient::instance().CreateSnapshot(taskinfo->task_id, info);
    if (result.errcode != ERR_SUCCESS) {
        taskinfo->status = TS_CreateSnapshotError;
        TaskInfoMgr::instance().update(taskinfo);
        TASK_LOG_ERROR(taskinfo->task_id, "create task snapshot:" << info->name << " failed");
        SnapshotManager::instance().updateCreatingSnapshot(taskinfo->task_id, result.errcode, result.errmsg);
    } else {
        // task will be closed after the snapshot is created whether it is running or not.
        taskinfo->status = TS_ShutOff;
        TaskInfoMgr::instance().update(taskinfo);
        remove_iptable_from_system(taskinfo->task_id);
        TASK_LOG_INFO(taskinfo->task_id, "create task snapshot:" << info->name << " successful, description:" << info->description);
        SnapshotManager::instance().delCreatingSnapshot(taskinfo->task_id);
        std::shared_ptr<dbc::snapshotInfo> newInfo = VmClient::instance().GetDomainSnapshot(taskinfo->task_id, info->name);
        if (newInfo) {
            SnapshotManager::instance().addTaskSnapshot(taskinfo->task_id, newInfo);
        } else {
            TASK_LOG_ERROR(taskinfo->task_id, "can not find snapshot:" << info->name);
        }
    }
}

void TaskManager::prune_task_thread_func() {
    while (m_running) {
		std::unique_lock<std::mutex> lock(m_prune_mtx);
		m_prune_cond.wait_for(lock, std::chrono::seconds(900), [this] {
			return !m_running || !m_process_tasks.empty();
		});

		if (!m_running)
			break;

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
                        TASK_LOG_INFO(task_id, "stop task and machine status is " << machine_status);
                        close_task(task_id);
                    }
                }
            } else if (machine_status == "waitingFulfill" || machine_status == "online") {
                std::vector<std::string> ids = it.second->task_ids;
                for (auto &task_id: ids) {
                    if (task_id.find("vm_check_") == std::string::npos) {
                        TASK_LOG_INFO(task_id, "stop task and machine status is " << machine_status);
                        close_task(task_id);
                    } else {
                        TASK_LOG_INFO(task_id, "delete check task and machine status is " << machine_status);
                        delete_task(task_id);
                    }
                }
            } else if (machine_status == "creating" || machine_status == "rented") {
                std::vector<std::string> ids = it.second->task_ids;
                for (auto &task_id: ids) {
                    if (task_id.find("vm_check_") != std::string::npos) {
                        TASK_LOG_INFO(task_id, "delete check task and machine status is " << machine_status);
                        delete_task(task_id);
                    }
                }
            }

            if (machine_status == "waitingFulfill" || machine_status == "online" ||
                machine_status == "creating" || machine_status == "rented") {
                int64_t rent_end = m_httpclient.request_rent_end(it.first);
                if (rent_end < 0) {
                    std::vector<std::string> ids = it.second->task_ids;
                    for (auto &task_id: ids) {
                        TASK_LOG_INFO(task_id, "stop task and machine status: " << machine_status <<
                                               " ,request rent end: " << rent_end);
                        close_task(task_id);
                    }

                    // 1小时出120个块
                    int64_t wallet_rent_end = it.second->rent_end;
                    int64_t reserve_end = wallet_rent_end + 120 * 24 * 10; //保留10天
                    int64_t cur_block = m_httpclient.request_cur_block();
                    if (cur_block > 0 && reserve_end < cur_block) {
                        ids = it.second->task_ids;
                        for (auto &task_id: ids) {
                            TASK_LOG_INFO(task_id, "delete task and machine status: " << machine_status << 
                                                   " ,task rent end: " << it.second->rent_end << 
                                                   " ,current block:" << cur_block);
                            delete_task(task_id);
                        }
                    }
                } else if (rent_end > 0) {
                    if (rent_end > it.second->rent_end) {
                        int64_t old_rent_end = it.second->rent_end;
                        it.second->rent_end = rent_end;
                        WalletRentTaskMgr::instance().updateRentEnd(it.first, rent_end);
                        LOG_INFO << "update rent_end, wallet=" << it.first
                            << ", update rent_end=old:" << old_rent_end << ",new:" << rent_end;
                    }
                } else {
                    LOG_INFO << "request_rent_end return 0: wallet=" << it.first;
                }
            }
        }
    }
}

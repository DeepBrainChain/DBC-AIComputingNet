#include "TaskManager.h"

#include <shadow.h>

#include <boost/format.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <regex>

#include "config/conf_manager.h"
#include "detail/VxlanManager.h"
#include "detail/cpu/CpuTuneManager.h"
#include "detail/rent_order/RentOrderManager.h"
#include "log/log.h"
#include "rapidjson/document.h"
#include "rapidjson/error/en.h"
#include "rapidjson/error/error.h"
#include "rapidjson/rapidjson.h"
#include "rapidjson/writer.h"
#include "service/peer_request_service/p2p_lan_service.h"
#include "tinyxml2.h"
#include "util/crypto/utilstrencodings.h"
#include "util/system_info.h"

FResult TaskManager::init() {
    FResult fret = VmClient::instance().init();
    if (fret.errcode != ERR_SUCCESS) {
        return fret;
    }

    fret = CpuTuneManager::instance().Init();
    if (fret.errcode != ERR_SUCCESS) {
        return fret;
    }

    fret = TaskInfoMgr::instance().init();
    if (fret.errcode != ERR_SUCCESS) {
        return FResult(ERR_ERROR, "taskinfo manager init failed");
    }

    auto taskinfos = TaskInfoMgr::instance().getAllTaskInfos();
    for (auto& it : taskinfos) {
        dbclog::instance().add_task_log_backend(it.second->getTaskId());
    }

    std::vector<std::string> taskids = TaskInfoMgr::instance().getAllTaskIds();
    fret = TaskDiskMgr::instance().init(taskids);
    if (fret.errcode != ERR_SUCCESS) {
        return FResult(ERR_ERROR, "taskdisk manager init failed");
    }

    fret = TaskGpuMgr::instance().init(taskids);
    if (fret.errcode != ERR_SUCCESS) {
        return FResult(ERR_ERROR, "taskgpu manager init failed");
    }

    fret = TaskIptableMgr::instance().init();
    if (fret.errcode != ERR_SUCCESS) {
        return FResult(ERR_ERROR, "taskiptable manager init failed");
    }

    fret = WalletRentTaskMgr::instance().init();
    if (fret.errcode != ERR_SUCCESS) {
        return FResult(ERR_ERROR, "wallet_renttask manager init failed");
    }

    fret = RentOrderManager::instance().Init();
    if (fret.errcode != ERR_SUCCESS) {
        return FResult(ERR_ERROR, "rent order manager init failed");
    }

    // 重启时恢复running_tasks、初始化task状态
    fret = this->init_tasks_status();
    if (fret.errcode != ERR_SUCCESS) {
        return FResult(ERR_ERROR, "restore tasks failed");
    }

    // 删除iptable中的reject规则
    shell_remove_reject_iptable_from_system();

    // UDP 广播租用通知
    m_udp_fd = socket(PF_INET, SOCK_DGRAM, 0);
    int optval = 1;
    setsockopt(m_udp_fd, SOL_SOCKET, SO_BROADCAST | SO_REUSEADDR, &optval,
               sizeof(int));

    m_running = true;
    if (m_process_thread == nullptr) {
        m_process_thread =
            new std::thread(&TaskManager::process_task_thread_func, this);
    }
    if (m_prune_thread == nullptr) {
        m_prune_thread =
            new std::thread(&TaskManager::prune_task_thread_func, this);
    }

    LOG_INFO << "init task manager successful";
    return FResultOk;
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

    if (m_udp_fd != -1) {
        close(m_udp_fd);
    }
    LOG_INFO << "TaskManager exited";
}

void TaskManager::broadcast_message(const std::string& msg) {
    if (m_udp_fd == -1) {
        m_udp_fd = socket(PF_INET, SOCK_DGRAM, 0);
        int optval = 1;
        setsockopt(m_udp_fd, SOL_SOCKET, SO_BROADCAST | SO_REUSEADDR, &optval,
                   sizeof(int));
    }

    if (m_udp_fd != -1) {
        // json by base64
        // { "node_id": "xxx", "status": "renting" }
        // { "node_id": "xxx", "status": "empty" }
        std::string strjson = "{\"node_id\":\"" +
                              ConfManager::instance().GetNodeId() +
                              "\", \"status\":\"" + msg + "\"}";
        std::string strbase64 =
            base64_encode((unsigned char*)strjson.c_str(), strjson.size());

        struct sockaddr_in theirAddr;
        memset(&theirAddr, 0, sizeof(struct sockaddr_in));
        theirAddr.sin_family = AF_INET;
        theirAddr.sin_addr.s_addr = inet_addr("255.255.255.255");
        theirAddr.sin_port = htons(55555);

        int sendBytes;
        if ((sendBytes = sendto(m_udp_fd, strbase64.c_str(), strbase64.size(),
                                0, (struct sockaddr*)&theirAddr,
                                sizeof(struct sockaddr))) == -1) {
            LOG_ERROR << "udp broadcast fail, errno=" << errno;
        }
    }
}

FResult TaskManager::init_tasks_status() {
    // supplement renter wallet of vm task
    auto renttask = WalletRentTaskMgr::instance().getAllWalletRentTasks();
    for (const auto& iter : renttask) {
        auto ids = iter.second->getTaskIds();
        for (auto& task_id : ids) {
            auto taskinfo = TaskInfoMgr::instance().getTaskInfo(task_id);
            if (taskinfo && taskinfo->getRenterWallet().empty()) {
                taskinfo->setRenterWallet(iter.first);
                TaskInfoMgr::instance().update(taskinfo);
            }
        }
    }

    // vda root backfile
    do {
        auto taskinfos = TaskInfoMgr::instance().getAllTaskInfos();
        for (auto& iter : taskinfos) {
            virDomainState st =
                VmClient::instance().GetDomainStatus(iter.first);
            if (st == VIR_DOMAIN_SHUTOFF) {
                if (iter.second->getVdaRootBackfile().empty()) {
                    std::string vda_backfile =
                        "/data/" + iter.second->getImageName();
                    std::string cmd =
                        "qemu-img info " + vda_backfile +
                        " | grep -i 'backing file:' | awk -F ': ' '{print $2}'";
                    std::string back_file = run_shell(cmd);
                    std::string root_backfile = vda_backfile;
                    while (!back_file.empty()) {
                        root_backfile = back_file;

                        cmd = "qemu-img info " + back_file +
                              " | grep -i 'backing file:' | awk -F ': ' "
                              "'{print $2}'";
                        back_file = run_shell(cmd);
                    }
                    iter.second->setVdaRootBackfile(root_backfile);
                    TaskInfoMgr::instance().update(iter.second);
                }
            }
        }
    } while (0);

    // restore tasks
    auto running_tasks = TaskInfoMgr::instance().getRunningTasks();
    for (auto& task_id : running_tasks) {
        auto taskinfo = TaskInfoMgr::instance().getTaskInfo(task_id);
        if (taskinfo != nullptr) {
            taskinfo->setTaskStatus(TaskStatus::TS_Task_Running);
            start_task(task_id);
        }
    }

    // init task's status
    auto taskinfos = TaskInfoMgr::instance().getAllTaskInfos();
    for (auto& iter : taskinfos) {
        if (iter.second->getTaskStatus() == TaskStatus::TS_None) {
            virDomainState st =
                VmClient::instance().GetDomainStatus(iter.first);
            if (st == VIR_DOMAIN_SHUTOFF || st == VIR_DOMAIN_SHUTDOWN ||
                st == VIR_DOMAIN_CRASHED || st == VIR_DOMAIN_NOSTATE) {
                iter.second->setTaskStatus(TaskStatus::TS_Task_Shutoff);
            } else if (st == VIR_DOMAIN_RUNNING || st == VIR_DOMAIN_PAUSED) {
                iter.second->setTaskStatus(TaskStatus::TS_Task_Running);
            } else if (st == VIR_DOMAIN_PMSUSPENDED) {
                iter.second->setTaskStatus(TaskStatus::TS_Task_PMSuspend);
            }
        }
    }

    return FResultOk;
}

void TaskManager::start_task(const std::string& task_id) {
    virDomainState vm_status = VmClient::instance().GetDomainStatus(task_id);
    if (vm_status == VIR_DOMAIN_RUNNING) return;

    if (VmClient::instance().StartDomain(task_id) == ERR_SUCCESS) {
        auto taskinfo = TaskInfoMgr::instance().getTaskInfo(task_id);
        if (taskinfo != nullptr) {
            taskinfo->setTaskStatus(TaskStatus::TS_Task_Running);
            TaskInfoManager::instance().update(taskinfo);
        }
        update_task_iptable(task_id);
        add_iptable_to_system(task_id);
        std::vector<unsigned int> cpuset;
        if (VmClient::instance().GetCpuTune(task_id, cpuset) > 0) {
            CpuTuneManager::instance().UseCpus(cpuset);
        }
        TASK_LOG_INFO(task_id, "start task successful");
    }
}

void TaskManager::close_task(const std::string& task_id) {
    virDomainState vm_status = VmClient::instance().GetDomainStatus(task_id);
    if (vm_status == VIR_DOMAIN_SHUTOFF) return;

    if (VmClient::instance().DestroyDomain(task_id) == ERR_SUCCESS) {
        auto taskinfo = TaskInfoMgr::instance().getTaskInfo(task_id);
        if (taskinfo != nullptr) {
            taskinfo->setTaskStatus(TaskStatus::TS_Task_Shutoff);
            TaskInfoManager::instance().update(taskinfo);
        }
        remove_iptable_from_system(task_id);
        std::vector<unsigned int> cpuset;
        if (VmClient::instance().GetCpuTune(task_id, cpuset) > 0) {
            CpuTuneManager::instance().UnuseCpus(cpuset);
        }
        TASK_LOG_INFO(task_id, "close task successful");
    }
}

void TaskManager::remove_iptable_from_system(const std::string& task_id) {
    auto iptableinfo = TaskIptableMgr::instance().getIptableInfo(task_id);
    if (iptableinfo != nullptr) {
        std::string host_ip = iptableinfo->getHostIP();
        uint16_t ssh_port = iptableinfo->getSSHPort();
        uint16_t rdp_port = iptableinfo->getRDPPort();
        std::vector<std::string> custom_port = iptableinfo->getCustomPort();
        std::string task_local_ip = iptableinfo->getTaskLocalIP();
        std::string public_ip = iptableinfo->getPublicIP();

        shell_remove_iptable_from_system(task_id, host_ip, ssh_port,
                                         task_local_ip, public_ip);
    }
}

void TaskManager::add_iptable_to_system(const std::string& task_id) {
    auto iptableinfo = TaskIptableMgr::instance().getIptableInfo(task_id);
    if (iptableinfo != nullptr) {
        std::string host_ip = iptableinfo->getHostIP();
        uint16_t ssh_port = iptableinfo->getSSHPort();
        uint16_t rdp_port = iptableinfo->getRDPPort();
        std::vector<std::string> custom_port = iptableinfo->getCustomPort();
        std::string task_local_ip = iptableinfo->getTaskLocalIP();
        std::string public_ip = iptableinfo->getPublicIP();

        shell_add_iptable_to_system(task_id, host_ip, ssh_port, rdp_port,
                                    custom_port, task_local_ip, public_ip);
    }
}

void TaskManager::shell_remove_iptable_from_system(
    const std::string& task_id, const std::string& host_ip, uint16_t ssh_port,
    const std::string& task_local_ip, const std::string& public_ip) {
    auto func_remove_chain = [](const std::string main_chain,
                                const std::string task_chain) {
        std::string cmd = "sudo iptables -t nat -F " + task_chain;
        run_shell(cmd);
        cmd = "sudo iptables -t nat -D " + main_chain + " -j " + task_chain;
        run_shell(cmd);
        cmd = "sudo iptables -t nat -X " + task_chain;
        run_shell(cmd);
    };

    // remove chain
    func_remove_chain("PREROUTING", "chain_" + task_id);
    func_remove_chain("PREROUTING", task_id + "_dnat");
    func_remove_chain("POSTROUTING", task_id + "_snat");

    if (!public_ip.empty()) {
        std::string nic_name = get_default_network_device();
        if (!nic_name.empty()) {
            std::string cmd =
                "sudo ip address del " + public_ip + " dev " + nic_name;
            run_shell(cmd);
        }
    }

    // remove old rules
    std::string cmd =
        "sudo iptables --table nat -D PREROUTING --protocol tcp "
        "--destination " +
        host_ip + " --destination-port " + std::to_string(ssh_port) +
        " --jump DNAT --to-destination " + task_local_ip + ":22";
    run_shell(cmd.c_str());

    cmd = "sudo iptables -t nat -D PREROUTING -p tcp --dport " +
          std::to_string(ssh_port) + " -j DNAT --to-destination " +
          task_local_ip + ":22";
    run_shell(cmd.c_str());

    auto pos = task_local_ip.rfind('.');
    std::string ip = task_local_ip.substr(0, pos) + ".1";
    cmd = "sudo iptables -t nat -D POSTROUTING -p tcp --dport " +
          std::to_string(ssh_port) + " -d " + task_local_ip + " -j SNAT --to " +
          ip;
    run_shell(cmd.c_str());

    cmd =
        "sudo iptables -t nat -D PREROUTING -p tcp -m tcp --dport 20000:60000 "
        "-j DNAT --to-destination " +
        task_local_ip + ":20000-60000";
    run_shell(cmd.c_str());

    cmd =
        "sudo iptables -t nat -D PREROUTING -p udp -m udp --dport 20000:60000 "
        "-j DNAT --to-destination " +
        task_local_ip + ":20000-60000";
    run_shell(cmd.c_str());

    cmd = "sudo iptables -t nat -D POSTROUTING -d " + task_local_ip +
          " -p tcp -m tcp --dport 20000:60000 -j SNAT --to-source " + host_ip;
    run_shell(cmd.c_str());

    cmd = "sudo iptables -t nat -D POSTROUTING -d " + task_local_ip +
          " -p udp -m udp --dport 20000:60000 -j SNAT --to-source " + host_ip;
    run_shell(cmd.c_str());

    cmd = "sudo iptables -t nat -D PREROUTING -p tcp -d " + host_ip +
          " --dport " + std::to_string(ssh_port) +
          " -j DNAT --to-destination " + task_local_ip + ":22";
    run_shell(cmd.c_str());

    pos = task_local_ip.rfind('.');
    ip = task_local_ip.substr(0, pos) + ".0/24";
    cmd = "sudo iptables -t nat -D POSTROUTING -s " + ip +
          " -j SNAT --to-source " + host_ip;
    run_shell(cmd.c_str());

    cmd = "sudo iptables -t nat -D PREROUTING -p tcp -d " + host_ip +
          " --dport 6000:60000 -j DNAT --to-destination " + task_local_ip +
          ":6000-60000";
    run_shell(cmd.c_str());
}

void TaskManager::shell_add_iptable_to_system(
    const std::string& task_id, const std::string& host_ip, uint16_t ssh_port,
    uint16_t rdp_port, const std::vector<std::string>& custom_port,
    const std::string& task_local_ip, const std::string& public_ip) {
    auto func_remove_chain = [](std::string main_chain,
                                std::string task_chain) {
        std::string cmd = "sudo iptables -t nat -F " + task_chain;
        run_shell(cmd);
        cmd = "sudo iptables -t nat -D " + main_chain + " -j " + task_chain;
        run_shell(cmd);
        cmd = "sudo iptables -t nat -X " + task_chain;
        run_shell(cmd);
    };

    // remove chain
    func_remove_chain("PREROUTING", "chain_" + task_id);
    func_remove_chain("PREROUTING", task_id + "_dnat");
    func_remove_chain("POSTROUTING", task_id + "_snat");

    std::string task_net_ip = public_ip.empty() ? host_ip : public_ip;

    std::string chain_name = task_id + "_dnat";

    // add chain and rules
    std::string cmd = "sudo iptables -t nat -N " + chain_name;
    run_shell(cmd);
    if (ssh_port > 0) {
        cmd = "sudo iptables -t nat -A " + chain_name +
              " -p tcp --destination " + task_net_ip + " --dport " +
              std::to_string(ssh_port) + " --jump DNAT --to-destination " +
              task_local_ip + ":22";
        run_shell(cmd);
    }
    if (rdp_port > 0) {
        cmd = "sudo iptables -t nat -A " + chain_name +
              " -p tcp --destination " + task_net_ip + " --dport " +
              std::to_string(rdp_port) + " --jump DNAT --to-destination " +
              task_local_ip + ":3389";
        run_shell(cmd);
    }

    for (auto& str : custom_port) {
        std::vector<std::string> v_protocol_port = util::split(str, ",");
        if (v_protocol_port.size() != 2 ||
            (v_protocol_port[0] != "tcp" && v_protocol_port[0] != "udp")) {
            continue;
        }

        std::string s_protocol = v_protocol_port[0];
        util::trim(s_protocol);
        std::string s_port = v_protocol_port[1];
        util::trim(s_port);

        if (util::is_digits(s_port)) {
            cmd = "sudo iptables -t nat -A " + chain_name + " -p " +
                  s_protocol + " --destination " + task_net_ip + " --dport " +
                  s_port + " --jump DNAT --to-destination " + task_local_ip +
                  ":" + s_port;
            run_shell(cmd);
            continue;
        }

        if (s_port.find(':') != std::string::npos) {
            std::vector<std::string> vec = util::split(s_port, ":");
            if (vec.size() == 2 && util::is_digits(vec[0]) &&
                util::is_digits(vec[1])) {
                cmd = "sudo iptables -t nat -A " + chain_name + " -p " +
                      s_protocol + " --destination " + task_net_ip +
                      " --dport " + vec[0] + " --jump DNAT --to-destination " +
                      task_local_ip + ":" + vec[1];
                run_shell(cmd);
                continue;
            }
        }

        if (s_port.find('-') != std::string::npos) {
            std::vector<std::string> vec = util::split(s_port, "-");
            if (vec.size() == 2 && util::is_digits(vec[0]) &&
                util::is_digits(vec[1])) {
                cmd = "sudo iptables -t nat -A " + chain_name + " -p " +
                      s_protocol + " --destination " + task_net_ip +
                      " --dport " + vec[0] + ":" + vec[1] +
                      " -j DNAT --to-destination " + task_local_ip + ":" +
                      vec[0] + "-" + vec[1];
                run_shell(cmd);
                continue;
            }
        }

        if (s_port.find(':') != std::string::npos &&
            s_port.find('-') != std::string::npos) {
            std::vector<std::string> vec = util::split(s_port, ":");
            if (vec.size() == 2) {
                std::vector<std::string> vec1 = util::split(vec[0], "-");
                std::vector<std::string> vec2 = util::split(vec[1], "-");
                if (vec1.size() == 2 && vec2.size() == 2) {
                    cmd = "sudo iptables -t nat -A " + chain_name + " -p " +
                          s_protocol + " --destination " + task_net_ip +
                          " --dport " + vec1[0] + ":" + vec1[1] +
                          " -j DNAT --to-destination " + task_local_ip + ":" +
                          vec2[0] + "-" + vec2[1];
                    run_shell(cmd);
                    continue;
                }
            }
        }
    }

    if (!public_ip.empty()) {
        cmd = "sudo iptables -t nat -A " + chain_name + " --destination " +
              public_ip + " --jump DNAT --to-destination " + task_local_ip;
        run_shell(cmd);
    }

    // add ref
    cmd = "sudo iptables -t nat -I PREROUTING -j " + chain_name;
    run_shell(cmd);

    if (!public_ip.empty()) {
        std::string chain_name_snat = task_id + "_snat";

        // add chain and rules
        cmd = "sudo iptables -t nat -N " + chain_name_snat;
        run_shell(cmd);
        cmd = "sudo iptables -t nat -I " + chain_name_snat + " --source " +
              task_local_ip + " --jump SNAT --to-source " + public_ip;
        run_shell(cmd);

        // add ref
        cmd = "sudo iptables -t nat -I POSTROUTING -j " + chain_name_snat;
        run_shell(cmd);

        std::string nic_name = get_default_network_device();
        if (!nic_name.empty()) {
            cmd = "sudo ip address add " + public_ip + " dev " + nic_name;
            run_shell(cmd);
        }
    }
}

void TaskManager::shell_remove_reject_iptable_from_system() {
    std::string cmd =
        "sudo iptables -t filter --list-rules |grep reject |awk '{print $0}' "
        "|tr \"\n\" \"|\"";
    std::string str = run_shell(cmd.c_str());
    std::vector<std::string> vec;
    util::split(str, "|", vec);
    for (auto& it : vec) {
        if (it.find("-A") != std::string::npos) {
            util::replace(it, "-A", "-D");
            std::string cmd = "sudo iptables -t filter " + it;
            run_shell(cmd.c_str());
        }
    }
}

void TaskManager::delete_task(const std::string& task_id) {
    unsigned int undefineFlags = 0;
    int32_t hasNvram = VmClient::instance().IsDomainHasNvram(task_id);
    if (hasNvram > 0) undefineFlags |= VIR_DOMAIN_UNDEFINE_NVRAM;

    // cputune
    std::vector<unsigned int> cpuset;
    if (VmClient::instance().GetCpuTune(task_id, cpuset) > 0)
        CpuTuneManager::instance().UnuseCpus(cpuset);

    // undefine task
    VmClient::instance().DestroyAndUndefineDomain(task_id, undefineFlags);

    auto taskinfo = TaskInfoMgr::instance().getTaskInfo(task_id);
    if (taskinfo == nullptr) return;

    // leave vxlan network
    p2p_lan_service::instance().send_network_leave_request(
        taskinfo->getNetworkName(), task_id);

    // delete taskinfo
    TaskInfoMgr::instance().del(task_id);

    // delete gpu
    TaskGpuManager::instance().del(task_id);

    // delete disk
    TaskDiskMgr::instance().delDisks(task_id);

    // delete snapshot
    TaskDiskMgr::instance().delSnapshots(task_id);

    // delete iptable
    remove_iptable_from_system(task_id);
    TaskIptableMgr::instance().del(task_id);

    // delete renttask
    WalletRentTaskMgr::instance().del(task_id);

    VmClient::instance().UndefineNWFilter(task_id);

    TASK_LOG_INFO(task_id, "delete task successful");
}

void TaskManager::clear_task_public_ip(const std::string& task_id) {
    auto taskinfoPtr = TaskInfoManager::instance().getTaskInfo(task_id);
    if (taskinfoPtr && !taskinfoPtr->getPublicIP().empty()) {
        TASK_LOG_INFO(task_id, "Recycle the public IP address: "
                                   << taskinfoPtr->getPublicIP());
        taskinfoPtr->setPublicIP("");
        TaskInfoManager::instance().update(taskinfoPtr);
    }

    auto taskIptablePtr =
        TaskIptableManager::instance().getIptableInfo(task_id);
    if (taskIptablePtr != nullptr && !taskIptablePtr->getPublicIP().empty()) {
        taskIptablePtr->setPublicIP("");
        TaskIptableManager::instance().update(taskIptablePtr);
    }
}

static std::string genpwd() {
    char chr[] = {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L',
                  'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
                  'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j',
                  'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
                  'w', 'x', 'y', 'z', '0', '1', '2', '3', '4', '5', '6', '7',
                  '8', '9', '_', '/', '+', '[', ']', '@', '#', '^'};

    struct timeval tv {};
    gettimeofday(&tv, nullptr);
    srand(tv.tv_usec);
    std::string strpwd;
    int idx0 = rand() % 52;
    char buf[2] = {0};
    sprintf(buf, "%c", chr[idx0]);
    strpwd.append(buf);

    for (int i = 0; i < 15; i++) {
        int idx0 = rand() % 70;
        sprintf(buf, "%c", chr[idx0]);
        strpwd.append(buf);
    }

    return strpwd;
}

std::shared_ptr<TaskInfo> TaskManager::findTask(const std::string& wallet,
                                                const std::string& task_id) {
    std::shared_ptr<TaskInfo> taskinfo = nullptr;
    auto renttask = WalletRentTaskMgr::instance().getWalletRentTask(wallet);
    if (renttask != nullptr) {
        auto ids = renttask->getTaskIds();
        for (auto& id : ids) {
            if (id == task_id) {
                taskinfo = TaskInfoMgr::instance().getTaskInfo(task_id);
                break;
            }
        }
    }

    return taskinfo;
}

// 创建task
FResult TaskManager::createTask(
    const std::string& wallet,
    const std::shared_ptr<dbc::node_create_task_req_data>& data, USER_ROLE role,
    std::string& task_id) {
    CreateTaskParams create_params;
    FResult fret = parse_create_params(data->additional, role, create_params,
                                       wallet, data->rent_order);
    if (fret.errcode != ERR_SUCCESS) {
        return fret;
    }

    int64_t tnow = time(nullptr);

    task_id = create_params.task_id;

    dbclog::instance().add_task_log_backend(task_id);

    // add taskinfo
    std::shared_ptr<TaskInfo> taskinfo = std::make_shared<TaskInfo>();
    taskinfo->setTaskId(task_id);
    taskinfo->setImageName(create_params.image_name);
    taskinfo->setDesc(create_params.desc);
    taskinfo->setLoginPassword(create_params.login_password);
    taskinfo->setSSHPort(create_params.ssh_port);
    taskinfo->setRDPPort(create_params.rdp_port);
    taskinfo->setCustomPort(create_params.custom_port);
    taskinfo->setCreateTime(tnow);
    taskinfo->setOperationSystem(create_params.operation_system);
    taskinfo->setBiosMode(create_params.bios_mode);
    taskinfo->setMulticast(create_params.multicast);
    taskinfo->setNetworkName(create_params.network_name);
    taskinfo->setPublicIP(create_params.public_ip);
    taskinfo->setNwfilter(create_params.nwfilter);
    taskinfo->setCpuSockets(create_params.cpu_sockets);
    taskinfo->setCpuCoresPerSocket(create_params.cpu_cores);
    taskinfo->setCpuThreadsPerCore(create_params.cpu_threads);
    taskinfo->setMemSize(create_params.mem_size);
    taskinfo->setVncPort(create_params.vnc_port);
    taskinfo->setVncPassword(create_params.vnc_password);
    taskinfo->setInterfaceModelType(create_params.interface_model_type);
    taskinfo->setOrderId(data->rent_order);
    taskinfo->setRenterWallet(wallet);
    taskinfo->setTaskStatus(TaskStatus::TS_Task_Creating);
    // vda root backfile
    std::string vda_backfile = "/data/" + create_params.image_name;
    std::string cmd = "qemu-img info " + vda_backfile +
                      " | grep -i 'backing file:' | awk -F ': ' '{print $2}'";
    std::string back_file = run_shell(cmd);
    std::string root_backfile = vda_backfile;
    while (!back_file.empty()) {
        root_backfile = back_file;

        cmd = "qemu-img info " + back_file +
              " | grep -i 'backing file:' | awk -F ': ' '{print $2}'";
        back_file = run_shell(cmd);
    }
    taskinfo->setVdaRootBackfile(root_backfile);
    TaskInfoMgr::instance().add(taskinfo);

    // add disks
    if (create_params.bios_mode != "pxe") {
        std::string disk_vda_file = "/data/vm_" +
                                    std::to_string(rand() % 100000) + "_" +
                                    util::time2str(tnow) + ".qcow2";
        std::shared_ptr<DiskInfo> disk_vda = std::make_shared<DiskInfo>();
        disk_vda->setName("vda");
        disk_vda->setSourceFile(disk_vda_file);
        disk_vda->setVirtualSize(g_disk_system_size * 1024L * 1024L * 1024L);
        TaskDiskMgr::instance().addDiskInfo(task_id, disk_vda);

        std::string disk_vdb_file;
        if (create_params.data_file_name.empty()) {
            disk_vdb_file = "/data/data_" + std::to_string(rand() % 100000) +
                            "_" + util::time2str(tnow) + ".qcow2";
        } else {
            disk_vdb_file = "/data/" + create_params.data_file_name;
        }
        std::shared_ptr<DiskInfo> disk_vdb = std::make_shared<DiskInfo>();
        disk_vdb->setName("vdb");
        disk_vdb->setSourceFile(disk_vdb_file);
        disk_vdb->setVirtualSize(create_params.disk_size * 1024L);
        TaskDiskMgr::instance().addDiskInfo(task_id, disk_vdb);
    }

    // add gpus
    auto& gpus = create_params.gpus;
    for (auto iter_gpu : gpus) {
        std::shared_ptr<GpuInfo> gpuinfo = std::make_shared<GpuInfo>();
        gpuinfo->setId(iter_gpu.first);
        for (auto& iter_device : iter_gpu.second) {
            gpuinfo->addDeviceId(iter_device);
        }
        TaskGpuMgr::instance().add(task_id, gpuinfo);
    }

    // add wallet_renttask
    int64_t rent_end =
        RentOrderManager::instance().GetRentEnd(data->rent_order, wallet);
    WalletRentTaskMgr::instance().add(wallet, task_id, rent_end);

    // send join network request
    p2p_lan_service::instance().send_network_join_request(
        create_params.network_name, task_id);

    // push event
    auto ev = std::make_shared<CreateTaskEvent>(task_id);
    ev->bios_mode = create_params.bios_mode;
    this->pushTaskEvent(ev);

    return FResultOk;
}

FResult TaskManager::parse_create_params(const std::string& additional,
                                         USER_ROLE role,
                                         CreateTaskParams& params,
                                         const std::string& wallet,
                                         const std::string& rent_order) {
    if (additional.empty()) {
        return FResult(ERR_ERROR, "additional is empty");
    }

    rapidjson::Document doc;
    doc.Parse(additional.c_str());
    if (!doc.IsObject()) {
        return FResult(ERR_ERROR, "additional parse failed");
    }

    std::string image_name, desc, operation_system, bios_mode, s_ssh_port,
        s_rdp_port, s_vnc_port, s_gpu_count, s_cpu_cores, s_mem_size,
        s_disk_size, data_file_name, network_name, public_ip,
        interface_model_type;
    std::vector<std::string> custom_ports, multicast, nwfilter;

    std::string task_id;
    std::string login_password;
    std::string vnc_password;
    int32_t cpu_cores = 0, sockets = 0, cores = 0, threads = 0;
    int32_t gpu_count = 0;
    std::map<std::string, std::list<std::string>> gpus;
    int64_t mem_size_k = 0;   // KB
    int64_t disk_size_k = 0;  // KB
    uint16_t n_ssh_port, n_rdp_port, n_vnc_port;

    // "task_id"
    JSON_PARSE_STRING(doc, "task_id", task_id);
    if (!task_id.empty()) {
        FResult fret = check_task_id(task_id);
        if (fret.errcode != ERR_SUCCESS) return fret;
    } else {
        task_id = util::create_task_id();
    }
    if (role == USER_ROLE::Verifier) {
        task_id = "vm_check_" + std::to_string(time(nullptr));
    }

    // rent order id
    bool in_order = !rent_order.empty();
    auto order_gpu_index =
        RentOrderManager::instance().GetRentedGpuIndex(rent_order, wallet);
    if (role == USER_ROLE::Verifier) {  // 验证人可能查不到 order_gpu_index
        order_gpu_index.clear();
        size_t sys_gpu_count = SystemInfo::instance().GetGpuInfo().size();
        for (int i = 0; i < sys_gpu_count; i++) order_gpu_index.push_back(i);
    }
    if (order_gpu_index.empty())
        return FResult(ERR_ERROR, "order gpu index is empty");

    // "gpu_count"
    JSON_PARSE_STRING(doc, "gpu_count", s_gpu_count);
    // check
    gpu_count = atoi(s_gpu_count.c_str());
    if (gpu_count < 0) {
        return FResult(ERR_ERROR,
                       "gpu_count is invalid (usage: gpu_count >= 0)");
    }
    if (role == USER_ROLE::Verifier) {
        gpu_count = SystemInfo::instance().GetGpuInfo().size();
    }

    if (gpu_count > order_gpu_index.size()) {
        return FResult(ERR_ERROR, "gpu_count exceeds number of leases");
    }

    float percent = 1.0f;
    if (in_order) {
        size_t sys_gpu_count = SystemInfo::instance().GetGpuInfo().size();
        if (sys_gpu_count < 1) return FResult(ERR_ERROR, "get gpu info failed");
        if (order_gpu_index.size() != sys_gpu_count) {
            if (gpu_count != order_gpu_index.size())
                return FResult(ERR_ERROR,
                               "only use the number what you exactly rented");
            percent = (float)gpu_count / sys_gpu_count;
        }
    }

    // 分配资源
    if (!allocate_gpu(gpu_count, gpus, order_gpu_index)) {
        return FResult(ERR_ERROR, "allocate gpu failed");
    }

    // "desc"
    JSON_PARSE_STRING(doc, "desc", desc);

    // "operation_system"
    JSON_PARSE_STRING(doc, "operation_system", operation_system);
    if (operation_system.empty()) {
        operation_system = "linux";
    }
    std::transform(operation_system.begin(), operation_system.end(),
                   operation_system.begin(), ::tolower);

    // check: "linux"、"windows"
    FResult fret = check_operation_system(operation_system);
    if (fret.errcode != ERR_SUCCESS) {
        return fret;
    }

    // "bios_mode"
    JSON_PARSE_STRING(doc, "bios_mode", bios_mode);
    if (bios_mode.empty()) {
        bios_mode = "legacy";
    }
    std::transform(bios_mode.begin(), bios_mode.end(), bios_mode.begin(),
                   ::tolower);
    // check: (1) linux: "legacy" (2) windows: “uefi” (3) linux or windows:
    // "pxe"
    fret = check_bios_mode(bios_mode);
    if (fret.errcode != ERR_SUCCESS) {
        return fret;
    }

    // “image_name”  扩展名为.qcow2：xxx.qcow2
    JSON_PARSE_STRING(doc, "image_name", image_name);
    fret = check_image(image_name);
    if (fret.errcode != ERR_SUCCESS) {
        return fret;
    }

    if (bios_mode != "pxe") {
        std::vector<std::string> image_files;
        getNeededBackingImage(image_name, image_files);
        if (!image_files.empty()) {
            std::string str = "image backfile not exist: ";
            for (size_t i = 0; i < image_files.size(); i++) {
                if (i > 0) str += ",";
                str += image_files[i];
            }

            return FResult(ERR_ERROR, str);
        }
    }

    // "public_ip"
    JSON_PARSE_STRING(doc, "public_ip", public_ip);
    // check
    fret = check_public_ip(public_ip);
    if (fret.errcode != ERR_SUCCESS) {
        return fret;
    }

    // "ssh_port“
    JSON_PARSE_STRING(doc, "ssh_port", s_ssh_port);
    // check
    n_ssh_port = (uint16_t)atoi(s_ssh_port.c_str());
    if (isLinuxOS(operation_system)) {
        // 配置了公网ip
        if (!public_ip.empty()) {
            fret = check_ssh_port(s_ssh_port);
            if (fret.errcode != ERR_SUCCESS) {
                n_ssh_port = 22;
            }
        }
        // 未配置公网ip
        else {
            fret = check_ssh_port(s_ssh_port);
            if (fret.errcode != ERR_SUCCESS) {
                return fret;
            } else {
                fret = check_port_conflict(n_ssh_port);
                if (fret.errcode != ERR_SUCCESS) {
                    return fret;
                }
            }
        }
    }

    // "rdp_port"
    JSON_PARSE_STRING(doc, "rdp_port", s_rdp_port);
    // check
    n_rdp_port = (uint16_t)atoi(s_rdp_port.c_str());
    if (isWindowsOS(operation_system)) {
        // 配置了公网ip
        if (!public_ip.empty()) {
            fret = check_rdp_port(s_rdp_port);
            if (fret.errcode != ERR_SUCCESS) {
                n_rdp_port = 3389;
            }
        }
        // 未配置公网ip
        else {
            fret = check_rdp_port(s_rdp_port);
            if (fret.errcode != ERR_SUCCESS) {
                return fret;
            } else {
                fret = check_port_conflict(n_rdp_port);
                if (fret.errcode != ERR_SUCCESS) {
                    return fret;
                }
            }
        }
    }

    // "vnc_port"
    JSON_PARSE_STRING(doc, "vnc_port", s_vnc_port);
    // check: range: [5900, 65535]
    n_vnc_port = (uint16_t)atoi(s_vnc_port.c_str());
    fret = check_vnc_port(s_vnc_port);
    if (fret.errcode != ERR_SUCCESS) {
        return fret;
    } else {
        fret = check_port_conflict(n_vnc_port);
        if (fret.errcode != ERR_SUCCESS) {
            return fret;
        }
    }

    // "cpu_cores"
    JSON_PARSE_STRING(doc, "cpu_cores", s_cpu_cores);
    // check
    cpu_cores = atoi(s_cpu_cores.c_str());
    if (cpu_cores <= 0) {
        return FResult(ERR_ERROR, "cpu_cores is invalid");
    }
    if (role == USER_ROLE::Verifier)
        cpu_cores = SystemInfo::instance().GetCpuInfo().cores;

    // 分配资源
    if (!allocate_cpu(cpu_cores, sockets, cores, threads, "", percent)) {
        return FResult(ERR_ERROR, "allocate cpu failed");
    }

    // "mem_size" (G)
    JSON_PARSE_STRING(doc, "mem_size", s_mem_size);
    // check
    mem_size_k = atoi(s_mem_size.c_str()) * 1024L * 1024L;
    if (mem_size_k <= 0) {
        return FResult(ERR_ERROR, "mem_size is invalid");
    }
    if (role == USER_ROLE::Verifier)
        mem_size_k = SystemInfo::instance().GetMemInfo().free;

    // 分配资源
    if (!allocate_mem(mem_size_k, "", percent)) {
        run_shell("echo 3 > /proc/sys/vm/drop_caches");

        if (!allocate_mem(mem_size_k, "", percent)) {
            return FResult(ERR_ERROR, "allocate mem failed");
        }
    }

    // "disk_size" (G)
    JSON_PARSE_STRING(doc, "disk_size", s_disk_size);
    if (bios_mode != "pxe") {
        // check
        disk_size_k = atoi(s_disk_size.c_str()) * 1024L * 1024L;
        if (disk_size_k <= 0) {
            return FResult(ERR_ERROR,
                           "disk_size is invalid (usage: disk_size > 0)");
        }

        if (role == USER_ROLE::Verifier) {
            disk_info _disk_info;
            SystemInfo::instance().GetDiskInfo("/data", _disk_info);

            int64_t disk_free =
                std::min(_disk_info.total - g_disk_system_size * 1024L * 1024L,
                         _disk_info.available);
            disk_free = std::max(0L, disk_free);

            disk_size_k = disk_free;
        }

        // 分配资源
        if (!allocate_disk(disk_size_k)) {
            return FResult(ERR_ERROR, "allocate disk failed");
        }

        // "data_file_name"
        JSON_PARSE_STRING(doc, "data_file_name", data_file_name);
        // check
        if (!data_file_name.empty()) {
            bfs::path data_path("/data/" + data_file_name);
            if (!boost::filesystem::exists(data_path)) {
                return FResult(ERR_ERROR,
                               "data_file_name is not exist, path: /data/" +
                                   data_file_name);
            }
        }
    }

    // "network_name"
    JSON_PARSE_STRING(doc, "network_name", network_name);
    // check
    if (!network_name.empty()) {
        FResult fret =
            VxlanManager::instance().CreateNetworkClient(network_name);
        if (fret.errcode != ERR_SUCCESS) {
            return fret;
        }
    }

    // "custom port"
    // [
    //    "tcp/udp,123",
    //    "tcp/udp,111:222",
    //    "tcp/udp,333-444",
    //    "tcp/udp,555-666:777-888"
    // ]
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

    // "multicast"
    // [
    //    "223.1.1.1",
    //    "224.1.1.1"
    // ]
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
    // check
    fret = check_multicast(multicast);
    if (fret.errcode != ERR_SUCCESS) {
        return fret;
    }

    // "nwfilter"
    // [
    //    "in,tcp,22,0.0.0.0/0,accept",
    //    "out,all,all,0.0.0.0/0,accept",
    //    "in,all,all,0.0.0.0/0,drop"
    // ]
    if (doc.HasMember("network_filters")) {
        const rapidjson::Value& v_nwfilters = doc["network_filters"];
        if (v_nwfilters.IsArray()) {
            for (rapidjson::SizeType i = 0; i < v_nwfilters.Size(); i++) {
                const rapidjson::Value& v_item = v_nwfilters[i];
                if (v_item.IsString()) {
                    std::string str = v_item.GetString();
                    nwfilter.push_back(str);
                }
            }
        }
    }

    // interface model type
    JSON_PARSE_STRING(doc, "interface_model_type", interface_model_type);
    if (interface_model_type.empty()) {
        interface_model_type = "virtio";
    } else if (interface_model_type != "e1000" &&
               interface_model_type != "rtl8139") {
        return FResult(ERR_ERROR, "invalid interface model type");
    }

    // password
    login_password = vnc_password = genpwd();
    // 从libvirt 8.0 开始，VNC密码限制最长8位
    // https://github.com/libvirt/libvirt/commit/27c1d06b5bd68bdce55efff0a50a15a30cb2a96b
    if (VmClient::instance().GetLibvirtVersion() >= 8000000) {
        if (vnc_password.length() > 8) {
            vnc_password = vnc_password.substr(0, 8);
        }
    }

    // return params
    params.task_id = task_id;
    params.login_password = login_password;
    params.image_name = image_name;
    params.desc = desc;
    params.operation_system = operation_system;
    params.bios_mode = bios_mode;
    params.ssh_port = n_ssh_port;
    params.rdp_port = n_rdp_port;
    params.custom_port = custom_ports;
    params.cpu_sockets = sockets;
    params.cpu_cores = cores;
    params.cpu_threads = threads;
    params.gpus = gpus;
    params.mem_size = mem_size_k;
    params.disk_size = disk_size_k;
    params.data_file_name = data_file_name;
    params.vnc_port = n_vnc_port;
    params.vnc_password = vnc_password;
    params.multicast = multicast;
    params.network_name = network_name;
    params.public_ip = public_ip;
    params.nwfilter = nwfilter;
    params.interface_model_type = interface_model_type;

    return FResultOk;
}

FResult TaskManager::check_task_id(const std::string& task_id) {
    if (task_id.length() < 20 || task_id.length() > 22)
        return FResult(ERR_ERROR, "wrong task id length");
    for (const auto& ch : task_id) {
        if (!isalnum(ch))
            return FResult(
                ERR_ERROR,
                "task id requires a combination of letters or numbers");
    }
    auto taskinfos = TaskInfoMgr::instance().getAllTaskInfos();
    if (taskinfos.find(task_id) != taskinfos.end())
        return FResult(ERR_ERROR, "task id already existed");
    return FResultOk;
}

FResult TaskManager::check_image(const std::string& image_name) {
    if (image_name.empty()) {
        return FResult(ERR_ERROR, "image_name is empty");
    }

    if (bfs::path(image_name).extension().string() != ".qcow2") {
        return FResult(ERR_ERROR, "'image_name' extension name is invalid");
    }

    // 镜像必须都存储在/data目录下
    bfs::path image_path("/data/" + image_name);
    if (!bfs::exists(image_path)) {
        return FResult(ERR_ERROR, "'image_name' is not exist (path:" +
                                      image_path.string() + ")");
    }

    if (ImageManager::instance().isDownloading(image_name) ||
        ImageManager::instance().isUploading(image_name)) {
        return FResult(
            ERR_ERROR,
            "'image_name':" + image_name +
                " in downloading or uploading, please try again later");
    }

    return FResultOk;
}

FResult TaskManager::check_operation_system(const std::string& os) {
    if (os.empty()) {
        return FResult(ERR_ERROR, "operation_system is emtpy");
    }

    if (isLinuxOS(os) || isWindowsOS(os)) {
        return FResultOk;
    } else {
        return FResult(ERR_ERROR, "unsupported operation system: " + os +
                                      " (usage: 'linux' 'windows')");
    }
}

FResult TaskManager::check_bios_mode(const std::string& bios_mode) {
    if (bios_mode.empty()) {
        return FResult(ERR_ERROR, "bios_mode is emtpy");
    }

    if (bios_mode == "legacy" || bios_mode == "uefi" || bios_mode == "pxe") {
        return FResultOk;
    } else {
        return FResult(ERR_ERROR,
                       "unsupported bios mode: " + bios_mode +
                           " (usage: 'legacy(default)'、'uefi'、'pxe')");
    }
}

FResult TaskManager::check_public_ip(const std::string& public_ip,
                                     const std::string& task_id) {
    if (public_ip.empty()) return FResultOk;
    ip_validator ip_vdr;
    variable_value val_ip(public_ip, false);
    if (!ip_vdr.validate(val_ip))
        return FResult(ERR_ERROR, "invalid public ip");
    if (SystemInfo::instance().GetDefaultRouteIp() == public_ip)
        return FResult(ERR_ERROR, "public ip being used");
    bool bFind = false;
    auto taskinfos = TaskInfoMgr::instance().getAllTaskInfos();
    for (const auto& iter : taskinfos) {
        if (iter.first == task_id) continue;
        if (iter.second->getPublicIP() == public_ip) {
            bFind = true;
            break;
        }
    }
    if (bFind) return FResult(ERR_ERROR, "public ip being used");
    return FResultOk;
}

FResult TaskManager::check_ssh_port(const std::string& s_port) {
    if (s_port.empty()) {
        return FResult(ERR_ERROR, "ssh_port is empty");
    }

    if (!util::is_digits(s_port)) {
        return FResult(ERR_ERROR, "ssh_port is not digit");
    }

    return FResultOk;
}

FResult TaskManager::check_rdp_port(const std::string& s_port) {
    if (s_port.empty()) {
        return FResult(ERR_ERROR, "rdp_port is empty");
    }

    if (!util::is_digits(s_port)) {
        return FResult(ERR_ERROR, "rdp_port is not digit");
    }

    return FResultOk;
}

FResult TaskManager::check_vnc_port(const std::string& s_port) {
    if (s_port.empty()) {
        return FResult(ERR_ERROR, "vnc_port is empty");
    }

    if (!util::is_digits(s_port)) {
        return FResult(ERR_ERROR, "vnc_port is not digit");
    }

    uint16_t n_port = (uint16_t)atoi(s_port.c_str());
    if (n_port < 5900 || n_port > 65535) {
        return FResult(ERR_ERROR, "vnc_port is invalid (usage: [5900, 65535])");
    }

    return FResultOk;
}

FResult TaskManager::check_port_conflict(
    uint16_t port, const std::string& exclude_task_id /* = "" */) {
    FResult fret = FResultOk;
    auto taskinfos = TaskInfoMgr::instance().getAllTaskInfos();
    for (const auto& iter : taskinfos) {
        if (iter.first == exclude_task_id) continue;

        // running
        virDomainState domain_status =
            VmClient::instance().GetDomainStatus(iter.first);
        if (domain_status != VIR_DOMAIN_RUNNING &&
            domain_status != VIR_DOMAIN_BLOCKED &&
            domain_status != VIR_DOMAIN_PAUSED)
            continue;

        // VNC server 占用的是宿主机的端口，虚拟机有公网IP也不能冲突
        if (iter.second->getVncPort() == port) {
            fret = FResult(ERR_ERROR, "port conflict, exist vnc_port = " +
                                          std::to_string(port));
            break;
        }

        if (!iter.second->getPublicIP().empty()) continue;

        if (iter.second->getSSHPort() == port) {
            fret = FResult(ERR_ERROR, "port conflict, exist ssh_port = " +
                                          std::to_string(port));
            break;
        } else if (iter.second->getRDPPort() == port) {
            fret = FResult(ERR_ERROR, "port conflict, exist rdp_port = " +
                                          std::to_string(port));
            break;
        }
    }

    return fret;
}

FResult TaskManager::check_multicast(
    const std::vector<std::string>& multicast) {
    for (const auto& address : multicast) {
        std::vector<std::string> vecSplit = util::split(address, ":");
        if (vecSplit.size() != 2) {
            return FResult(ERR_ERROR, "multicast format: ip:port");
        }

        boost::asio::ip::address addrip =
            boost::asio::ip::address_v4::from_string(vecSplit[0]);
        if (!addrip.is_multicast()) {
            return FResult(ERR_ERROR, "address is not a multicast address");
        }

        port_validator port_vdr;
        variable_value val_port(vecSplit[1], false);
        if (!port_vdr.validate(val_port)) {
            return FResult(ERR_ERROR, "invalid multicast port");
        }
    }

    return FResultOk;
}

bool TaskManager::allocate_cpu(int32_t& total_cores, int32_t& sockets,
                               int32_t& cores_per_socket, int32_t& threads,
                               const std::string& exclude_task_id,
                               float percent) {
    int32_t nTotalCores = SystemInfo::instance().GetCpuInfo().cores;
    if (total_cores > floor(nTotalCores * percent)) return false;

    int32_t nUsedCores = 0;
    auto taskinfos = TaskInfoMgr::instance().getAllTaskInfos();
    for (auto& iter : taskinfos) {
        if (iter.first == exclude_task_id) continue;
        virDomainState st = VmClient::instance().GetDomainStatus(iter.first);
        if (st == VIR_DOMAIN_RUNNING) {
            nUsedCores += iter.second->getTotalCores();
        }
    }
    if (total_cores + nUsedCores > nTotalCores) return false;

    int32_t nSockets = SystemInfo::instance().GetCpuInfo().physical_cpus;
    int32_t nCores = SystemInfo::instance().GetCpuInfo().physical_cores_per_cpu;
    int32_t nThreads = SystemInfo::instance().GetCpuInfo().threads_per_cpu;

    if (total_cores % (nThreads * nSockets) == 0 &&
        total_cores > nCores * nThreads) {
        sockets = nSockets;
        threads = nThreads;
    } else {
        sockets = 1;
    }

    if (total_cores % (sockets * nThreads) == 0) {
        threads = nThreads;
    } else {
        threads = 1;
    }

    cores_per_socket = total_cores / (sockets * threads);
    if (sockets * cores_per_socket * threads == total_cores) return true;
    return false;
    // int32_t ret_total_cores = total_cores;
    // threads = nThreads;

    // for (int n = 1; n <= nSockets; n++) {
    //     int32_t tmp_cores = total_cores;
    //     while (tmp_cores % (n * threads) != 0)
    //         tmp_cores += 1;

    //     if (tmp_cores / (n * threads) <= nCores) {
    //         sockets = n;
    //         cores_per_socket = tmp_cores / (n * threads);
    //         ret_total_cores = tmp_cores;
    //         break;
    //     }
    // }

    // if (ret_total_cores == sockets * cores_per_socket * threads) {
    //     total_cores = ret_total_cores;
    //     return true;
    // }
    // else {
    //     return false;
    // }
}

bool TaskManager::allocate_mem(int64_t mem_size_k,
                               const std::string& exclude_task_id,
                               float percent) {
    if (mem_size_k <= 0) return false;
    auto meminfo = SystemInfo::instance().GetMemInfo();
    if (mem_size_k > meminfo.free) return false;
    if (mem_size_k > floor(meminfo.total * percent)) return false;
    int64_t mem_used = 0;
    auto taskinfos = TaskInfoMgr::instance().getAllTaskInfos();
    for (auto& iter : taskinfos) {
        if (iter.first == exclude_task_id) continue;
        virDomainState st = VmClient::instance().GetDomainStatus(iter.first);
        if (st == VIR_DOMAIN_RUNNING) {
            mem_used += iter.second->getMemSize();
        }
    }
    if (mem_size_k + mem_used > meminfo.total) return false;
    return true;
}

bool TaskManager::allocate_gpu(
    int32_t gpu_count, std::map<std::string, std::list<std::string>>& gpus,
    std::vector<int32_t> gpu_order, const std::string& exclude_task_id) {
    if (gpu_order.empty()) return false;
    if (gpu_count > gpu_order.size()) return false;

    std::map<std::string, gpu_info> can_use_gpu =
        SystemInfo::instance().GetGpuInfo();
    if (gpu_count == 0) {
        if (gpu_order.size() == can_use_gpu.size())
            return true;
        else
            return false;
    }

    std::set<std::string> used_gpus;
    auto taskinfos = TaskInfoMgr::instance().getAllTaskInfos();
    for (auto& iter : taskinfos) {
        if (iter.first == exclude_task_id) continue;
        virDomainState st = VmClient::instance().GetDomainStatus(iter.first);
        if (st != VIR_DOMAIN_SHUTOFF) {
            auto task_gpus = TaskGpuMgr::instance().getTaskGpus(iter.first);
            for (auto& task_gpu : task_gpus) {
                if (used_gpus.count(task_gpu.first) == 0)
                    used_gpus.insert(task_gpu.first);
            }
        }
    }

    int index = 0;
    for (const auto& it : can_use_gpu) {
        auto ifind = std::find(gpu_order.begin(), gpu_order.end(), index);
        if (ifind != gpu_order.end() && used_gpus.count(it.first) == 0) {
            gpus[it.first] = it.second.devices;
        }
        index++;
        if (gpus.size() == gpu_count) break;
    }

    return gpus.size() == gpu_count;
}

bool TaskManager::allocate_disk(int64_t disk_size_k) {
    disk_info _disk_info;
    SystemInfo::instance().GetDiskInfo("/data", _disk_info);
    // /data 的剩余大小要减掉系统盘大小
    int64_t disk_free =
        std::min(_disk_info.total - g_disk_system_size * 1024L * 1024L,
                 _disk_info.available);

    return disk_size_k > 0 && disk_size_k <= disk_free;
}

// 启动task
FResult TaskManager::startTask(const std::string& wallet,
                               const std::string& rent_order,
                               const std::string& task_id) {
    auto taskinfo = TaskInfoMgr::instance().getTaskInfo(task_id);
    if (taskinfo == nullptr) {
        return FResult(ERR_ERROR, "task not exist");
    }

    if (!VmClient::instance().IsExistDomain(task_id)) {
        return FResult(ERR_ERROR, "task not exist");
    }

    // check resource
    FResult fret = check_resource(taskinfo, rent_order);
    if (fret.errcode != ERR_SUCCESS) {
        return fret;
    }

    virDomainState vm_status = VmClient::instance().GetDomainStatus(task_id);
    if (vm_status == VIR_DOMAIN_NOSTATE || vm_status == VIR_DOMAIN_SHUTOFF ||
        vm_status == VIR_DOMAIN_PMSUSPENDED) {
        taskinfo->setTaskStatus(TaskStatus::TS_Task_Starting);

        auto ev = std::make_shared<StartTaskEvent>(task_id);
        this->pushTaskEvent(ev);

        return FResultOk;
    } else {
        return FResult(ERR_ERROR, "task is " + vm_status_string(vm_status));
    }
}

FResult TaskManager::check_cpu(int32_t sockets, int32_t cores, int32_t threads,
                               const std::string& task_id) {
    int32_t cpu_cores = sockets * cores * threads;
    if (cpu_cores <= 0 ||
        cpu_cores > SystemInfo::instance().GetCpuInfo().cores ||
        sockets > SystemInfo::instance().GetCpuInfo().physical_cpus ||
        cores > SystemInfo::instance().GetCpuInfo().physical_cores_per_cpu ||
        threads > SystemInfo::instance().GetCpuInfo().threads_per_cpu) {
        return FResult(ERR_ERROR, "check cpu failed");
    }

    int32_t used_cpus = 0;
    bool cpu_conflict = false;

    std::vector<unsigned char> cpumap;
    cpumap.resize(CpuTuneManager::instance().GetCpuMapLength());
    auto taskinfos = TaskInfoMgr::instance().getAllTaskInfos();
    for (auto& iter : taskinfos) {
        if (iter.first == task_id) continue;
        virDomainState st = VmClient::instance().GetDomainStatus(iter.first);
        if (st == VIR_DOMAIN_RUNNING) {
            used_cpus += iter.second->getTotalCores();
            std::vector<unsigned int> cpuset;
            VmClient::instance().GetCpuTune(iter.first, cpuset);
            for (const auto& cpu : cpuset) {
                VIR_USE_CPU(&cpumap[0], cpu);
            }
        }
    }

    if (used_cpus + cpu_cores > SystemInfo::instance().GetCpuInfo().cores)
        return FResult(ERR_ERROR, "cpu count exceeds the maximum");

    std::vector<unsigned int> cpuset;
    if (VmClient::instance().GetCpuTune(task_id, cpuset) > 0) {
        for (const auto& cpu : cpuset) {
            if (VIR_CPU_USED(&cpumap[0], cpu)) {
                cpu_conflict = true;
                break;
            }
        }
    } else {
        return FResult(ERR_ERROR, "get cpu tune failed");
    }
    if (!cpu_conflict) return FResultOk;

    // VCPU绑定出现冲突，重新绑定
    CpuTuneManager::instance().UpdateCpuMap(cpumap);
    return VmClient::instance().ResetCpuTune(task_id, cpu_cores);
}

FResult TaskManager::check_gpu(
    const std::map<std::string, std::shared_ptr<GpuInfo>>& gpus,
    const std::string& exclude_task_id) {
    std::map<std::string, gpu_info> can_use_gpu =
        SystemInfo::instance().GetGpuInfo();
    auto taskinfos = TaskInfoMgr::instance().getAllTaskInfos();
    for (auto& iter : taskinfos) {
        if (iter.first == exclude_task_id) continue;
        virDomainState st = VmClient::instance().GetDomainStatus(iter.first);
        if (st != VIR_DOMAIN_SHUTOFF) {
            auto _gpus = TaskGpuMgr::instance().getTaskGpus(iter.first);
            if (!_gpus.empty()) {
                for (auto& it1 : _gpus) {
                    can_use_gpu.erase(it1.first);
                }
            }
        }
    }

    FResult fret = FResultOk;
    for (auto& it : gpus) {
        auto it_gpu = can_use_gpu.find(it.first);
        if (it_gpu == can_use_gpu.end()) {
            fret = FResult(ERR_ERROR, "gpu " + it.first + " is already in use");
            break;
        }

        auto ids = it.second->getDeviceIds();
        for (auto& it_device : ids) {
            std::list<std::string>& can_use_devices = it_gpu->second.devices;
            auto found = std::find(can_use_devices.begin(),
                                   can_use_devices.end(), it_device);
            if (found == can_use_devices.end()) {
                fret = FResult(ERR_ERROR, "gpu device " + it_device +
                                              " not found in system");
                break;
            }
        }

        if (fret.errcode != ERR_SUCCESS) break;
    }

    return fret;
}

FResult TaskManager::check_mem(int64_t mem_size_k) {
    if (mem_size_k > 0 &&
        mem_size_k <= SystemInfo::instance().GetMemInfo().free) {
        return FResultOk;
    } else {
        return FResultError;
    }
}

FResult TaskManager::check_resource(const std::shared_ptr<TaskInfo>& taskinfo,
                                    const std::string& rent_order) {
    virDomainState dom_state =
        VmClient::instance().GetDomainStatus(taskinfo->getTaskId());
    // 1. 检查订单ID和对应的显卡序号，如果不一致，自动修正到新订单，此时
    //    默认新旧订单显卡数量一定相同，不相同的处理太麻烦，还要修改内存cpu等。
    // 2. 检查xml里的所有gpu是否都可用，如果存在不可用的gpu，自动删除
    TaskGpuMgr::instance().checkXmlGpu(taskinfo, rent_order);

    // cpu
    FResult fret =
        check_cpu(taskinfo->getCpuSockets(), taskinfo->getCpuCoresPerSocket(),
                  taskinfo->getCpuThreadsPerCore(), taskinfo->getTaskId());
    if (fret.errcode != ERR_SUCCESS) {
        return fret;
    }

    // gpu
    auto gpus = TaskGpuMgr::instance().getTaskGpus(taskinfo->getTaskId());
    fret = check_gpu(gpus, taskinfo->getTaskId());
    if (fret.errcode != ERR_SUCCESS) {
        return FResult(ERR_ERROR, "check gpu failed");
    }

    // mem
    if (dom_state != VIR_DOMAIN_RUNNING) {
        int64_t mem_size_k = taskinfo->getMemSize();
        fret = check_mem(mem_size_k);
        if (fret.errcode != ERR_SUCCESS) {
            run_shell("echo 3 > /proc/sys/vm/drop_caches");

            fret = check_mem(mem_size_k);
            if (fret.errcode != ERR_SUCCESS) {
                return FResult(ERR_ERROR, "check memory failed");
            }
        }
    }

    // ssh_port or rdp_port
    if (isLinuxOS(taskinfo->getOperationSystem())) {
        fret =
            check_port_conflict(taskinfo->getSSHPort(), taskinfo->getTaskId());
        if (fret.errcode != ERR_SUCCESS) {
            return FResult(ERR_ERROR, "check ssh_port failed");
        }
    } else if (isWindowsOS(taskinfo->getOperationSystem())) {
        fret =
            check_port_conflict(taskinfo->getRDPPort(), taskinfo->getTaskId());
        if (fret.errcode != ERR_SUCCESS) {
            return FResult(ERR_ERROR, "check rdp_port failed");
        }
    }

    // vnc_port
    fret = check_port_conflict(taskinfo->getVncPort(), taskinfo->getTaskId());
    if (fret.errcode != ERR_SUCCESS) {
        return FResult(ERR_ERROR, "check vnc_port failed");
    }

    // public_ip
    fret = check_public_ip(taskinfo->getPublicIP(), taskinfo->getTaskId());
    if (fret.errcode != ERR_SUCCESS) {
        return FResult(ERR_ERROR, fret.errmsg);
    }

    return FResultOk;
}

// 关闭task
FResult TaskManager::shutdownTask(const std::string& wallet,
                                  const std::string& rent_order,
                                  const std::string& task_id) {
    auto taskinfo = TaskInfoMgr::instance().getTaskInfo(task_id);
    if (taskinfo == nullptr) {
        return FResult(ERR_ERROR, "task not exist");
    }

    if (!VmClient::instance().IsExistDomain(task_id)) {
        return FResult(ERR_ERROR, "task not exist");
    }

    virDomainState vm_status = VmClient::instance().GetDomainStatus(task_id);
    if (vm_status != VIR_DOMAIN_SHUTOFF && vm_status != VIR_DOMAIN_SHUTDOWN) {
        taskinfo->setTaskStatus(TaskStatus::TS_Task_BeingShutdown);

        auto ev = std::make_shared<ShutdownTaskEvent>(task_id);
        this->pushTaskEvent(ev);

        return FResultOk;
    } else {
        return FResult(ERR_ERROR, "task is " + vm_status_string(vm_status));
    }
}

// 强制断电
FResult TaskManager::poweroffTask(const std::string& wallet,
                                  const std::string& rent_order,
                                  const std::string& task_id) {
    auto taskinfo = TaskInfoMgr::instance().getTaskInfo(task_id);
    if (taskinfo == nullptr) {
        return FResult(ERR_ERROR, "task not exist");
    }

    if (!VmClient::instance().IsExistDomain(task_id)) {
        return FResult(ERR_ERROR, "task not exist");
    }

    virDomainState vm_status = VmClient::instance().GetDomainStatus(task_id);
    if (vm_status != VIR_DOMAIN_SHUTOFF) {
        taskinfo->setTaskStatus(TaskStatus::TS_Task_BeingPoweroff);

        auto ev = std::make_shared<PowerOffTaskEvent>(task_id);
        this->pushTaskEvent(ev);

        return FResultOk;
    } else {
        return FResult(ERR_ERROR, "task is " + vm_status_string(vm_status));
    }
}

// 重启task
FResult TaskManager::restartTask(const std::string& wallet,
                                 const std::string& rent_order,
                                 const std::string& task_id,
                                 bool force_reboot) {
    auto taskinfo = TaskInfoMgr::instance().getTaskInfo(task_id);
    if (taskinfo == nullptr) {
        return FResult(ERR_ERROR, "task not exist");
    }

    if (!VmClient::instance().IsExistDomain(task_id)) {
        return FResult(ERR_ERROR, "task not exist");
    }

    // check resource
    FResult fret = check_resource(taskinfo, rent_order);
    if (fret.errcode != ERR_SUCCESS) {
        return fret;
    }

    virDomainState vm_status = VmClient::instance().GetDomainStatus(task_id);
    if (vm_status == VIR_DOMAIN_NOSTATE || vm_status == VIR_DOMAIN_SHUTOFF ||
        vm_status == VIR_DOMAIN_RUNNING) {
        taskinfo->setTaskStatus(TaskStatus::TS_Task_Restarting);

        if (force_reboot) {
            auto ev = std::make_shared<ForceRebootTaskEvent>(task_id);
            this->pushTaskEvent(ev);
        } else {
            auto ev = std::make_shared<ReStartTaskEvent>(task_id);
            this->pushTaskEvent(ev);
        }

        return FResultOk;
    } else {
        return FResult(ERR_ERROR, "task is " + vm_status_string(vm_status));
    }
}

// 重置task（慎用）
FResult TaskManager::resetTask(const std::string& wallet,
                               const std::string& rent_order,
                               const std::string& task_id) {
    auto taskinfo = TaskInfoMgr::instance().getTaskInfo(task_id);
    if (taskinfo == nullptr) {
        return FResult(ERR_ERROR, "task not exist");
    }

    if (!VmClient::instance().IsExistDomain(task_id)) {
        return FResult(ERR_ERROR, "task not exist");
    }

    virDomainState vm_status = VmClient::instance().GetDomainStatus(task_id);
    if (vm_status == VIR_DOMAIN_RUNNING) {
        taskinfo->setTaskStatus(TaskStatus::TS_Task_Resetting);

        auto ev = std::make_shared<ResetTaskEvent>(task_id);
        this->pushTaskEvent(ev);

        return FResultOk;
    } else {
        return FResult(ERR_ERROR, "task is " + vm_status_string(vm_status));
    }
}

// 删除task
FResult TaskManager::deleteTask(const std::string& wallet,
                                const std::string& rent_order,
                                const std::string& task_id) {
    auto taskinfo = TaskInfoMgr::instance().getTaskInfo(task_id);
    if (taskinfo == nullptr) {
        return FResult(ERR_ERROR, "task not exist");
    }

    if (!VmClient::instance().IsExistDomain(task_id)) {
        return FResult(ERR_ERROR, "task not exist");
    }

    taskinfo->setTaskStatus(TaskStatus::TS_Task_Deleting);

    auto ev = std::make_shared<DeleteTaskEvent>(task_id);
    this->pushTaskEvent(ev);

    return FResultOk;
}

// 修改task
FResult TaskManager::modifyTask(
    const std::string& wallet,
    const std::shared_ptr<dbc::node_modify_task_req_data>& data) {
    std::string task_id = data->task_id;
    auto taskinfoPtr = TaskInfoManager::instance().getTaskInfo(task_id);
    if (taskinfoPtr == nullptr) {
        return FResult(ERR_ERROR, "task not exist");
    }

    if (!VmClient::instance().IsExistDomain(task_id)) {
        return FResult(ERR_ERROR, "task not exist");
    }

    // virDomainState vm_status = VmClient::instance().GetDomainStatus(task_id);
    // if (vm_status != VIR_DOMAIN_SHUTOFF) {
    //     return FResult(ERR_ERROR, "task is running, please close it first");
    // }

    std::string rent_order = taskinfoPtr->getOrderId();
    auto order_gpu_index =
        RentOrderManager::instance().GetRentedGpuIndex(rent_order, wallet);
    size_t sys_gpu_count = SystemInfo::instance().GetGpuInfo().size();
    // 是否整机租用
    bool rent_all_machine =
        rent_order.empty() || order_gpu_index.size() == sys_gpu_count;
    float percent = 1.0f;
    if (!rent_all_machine) {
        if (order_gpu_index.empty())
            return FResult(ERR_ERROR, "gpu rent order is empty");
        percent = (float)order_gpu_index.size() / sys_gpu_count;
    }

    bool need_reset_iptables = false;
    bool need_reboot_vm = false;
    bool need_redefine_vm = false;

    rapidjson::Document doc;
    doc.Parse(data->additional.c_str());
    if (!doc.IsObject()) {
        return FResult(ERR_ERROR, "additional parse failed");
    }

    std::string new_public_ip;
    JSON_PARSE_STRING(doc, "new_public_ip", new_public_ip);
    FResult fret = check_public_ip(new_public_ip, task_id);
    if (fret.errcode != ERR_SUCCESS) {
        return fret;
    } else if (new_public_ip != taskinfoPtr->getPublicIP()) {
        auto taskIptablePtr =
            TaskIptableManager::instance().getIptableInfo(task_id);
        if (taskIptablePtr != nullptr) {
            need_reset_iptables = true;
            TASK_LOG_INFO(task_id, "modify task old public ip: "
                                       << taskinfoPtr->getPublicIP()
                                       << ", new public ip: " << new_public_ip);
            taskinfoPtr->setPublicIP(new_public_ip);
            TaskInfoManager::instance().update(taskinfoPtr);

            taskIptablePtr->setPublicIP(new_public_ip);
            TaskIptableManager::instance().update(taskIptablePtr);
        } else {
            return FResult(ERR_ERROR,
                           "can not find iptable info when modify public ip");
        }
    }

    uint16_t old_ssh_port, new_ssh_port;
    std::string s_new_ssh_port;
    JSON_PARSE_STRING(doc, "new_ssh_port", s_new_ssh_port);
    if (!s_new_ssh_port.empty()) {
        new_ssh_port = (uint16_t)atoi(s_new_ssh_port.c_str());
        if (new_ssh_port <= 0) {
            return FResult(ERR_ERROR, "new_ssh_port " + s_new_ssh_port +
                                          " is invalid! (usage: > 0)");
        }

        fret = check_port_conflict(new_ssh_port, task_id);
        if (taskinfoPtr->getPublicIP().empty() && fret.errcode != ERR_SUCCESS) {
            return FResult(ERR_ERROR, "new_ssh_port " + s_new_ssh_port +
                                          " has been used!");
        }

        old_ssh_port = taskinfoPtr->getSSHPort();
        if (new_ssh_port != old_ssh_port) {
            auto taskIptablePtr =
                TaskIptableManager::instance().getIptableInfo(task_id);
            if (taskIptablePtr != nullptr) {
                need_reset_iptables = true;
                taskinfoPtr->setSSHPort(new_ssh_port);
                TaskInfoMgr::instance().update(taskinfoPtr);

                taskIptablePtr->setSSHPort(new_ssh_port);
                TaskIptableManager::instance().update(taskIptablePtr);

                TASK_LOG_INFO(task_id, "modify task old ssh port: "
                                           << old_ssh_port << ", new ssh port: "
                                           << new_ssh_port);
            } else {
                return FResult(
                    ERR_ERROR,
                    "can not find iptable info when modify ssh port");
            }
        }
    }

    uint16_t new_rdp_port;
    std::string s_new_rdp_port;
    JSON_PARSE_STRING(doc, "new_rdp_port", s_new_rdp_port);
    if (!s_new_rdp_port.empty()) {
        new_rdp_port = (uint16_t)atoi(s_new_rdp_port.c_str());
        if (new_rdp_port <= 0) {
            return FResult(ERR_ERROR, "new_rdp_port " + s_new_rdp_port +
                                          " is invalid! (usage: > 0)");
        }

        fret = check_port_conflict(new_rdp_port, task_id);
        if (taskinfoPtr->getPublicIP().empty() && fret.errcode != ERR_SUCCESS) {
            return FResult(ERR_ERROR, "new_rdp_port " + s_new_rdp_port +
                                          " has been used!");
        }

        if (new_rdp_port != taskinfoPtr->getRDPPort()) {
            auto taskIptablePtr =
                TaskIptableManager::instance().getIptableInfo(task_id);
            if (taskIptablePtr != nullptr) {
                need_reset_iptables = true;
                TASK_LOG_INFO(task_id,
                              "modify task old rdp port: "
                                  << taskinfoPtr->getRDPPort()
                                  << ", new rdp port: " << new_rdp_port);

                taskinfoPtr->setRDPPort(new_rdp_port);
                TaskInfoMgr::instance().update(taskinfoPtr);

                taskIptablePtr->setRDPPort(new_rdp_port);
                TaskIptableManager::instance().update(taskIptablePtr);
            } else {
                return FResult(
                    ERR_ERROR,
                    "can not find iptable info when modify rdp port");
            }
        }
    }

    if (!s_new_ssh_port.empty() && !s_new_rdp_port.empty() &&
        new_ssh_port == new_rdp_port) {
        return FResult(ERR_ERROR,
                       "new_ssh_port and new_rdp_port are the same!");
    }

    std::vector<std::string> new_custom_port;
    if (doc.HasMember("new_custom_port")) {
        const rapidjson::Value& v_custom_port = doc["new_custom_port"];
        if (v_custom_port.IsArray()) {
            for (rapidjson::SizeType i = 0; i < v_custom_port.Size(); i++) {
                new_custom_port.push_back(v_custom_port[i].GetString());
            }

            if (new_custom_port != taskinfoPtr->getCustomPort()) {
                auto taskIptablePtr =
                    TaskIptableManager::instance().getIptableInfo(task_id);
                if (taskIptablePtr != nullptr) {
                    need_reset_iptables = true;
                    taskinfoPtr->setCustomPort(new_custom_port);
                    TaskInfoMgr::instance().update(taskinfoPtr);

                    taskIptablePtr->setCustomPort(new_custom_port);
                    TaskIptableManager::instance().update(taskIptablePtr);

                    TASK_LOG_INFO(task_id, "modify task custom port");
                } else {
                    return FResult(
                        ERR_ERROR,
                        "can not find iptable info when modify custom port");
                }
            }
        }
    }

    if (need_reset_iptables) {
        remove_iptable_from_system(task_id);
        add_iptable_to_system(task_id);
        TASK_LOG_INFO(task_id, "reset iptables successful");
    }

    std::vector<std::string> new_nwfiter;
    if (doc.HasMember("new_network_filters")) {
        const rapidjson::Value& v_nwfilters = doc["new_network_filters"];
        if (v_nwfilters.IsArray()) {
            for (rapidjson::SizeType i = 0; i < v_nwfilters.Size(); i++) {
                const rapidjson::Value& v_item = v_nwfilters[i];
                if (v_item.IsString()) {
                    std::string str = v_item.GetString();
                    new_nwfiter.push_back(str);
                }
            }

            if (new_nwfiter != taskinfoPtr->getNwfilter()) {
                TASK_LOG_INFO(task_id, "modify task network filter");
                taskinfoPtr->setNwfilter(new_nwfiter);
                TaskInfoManager::instance().update(taskinfoPtr);

                ERRCODE ret =
                    VmClient::instance().DefineNWFilter(task_id, new_nwfiter);
                if (ret != ERR_SUCCESS) {
                    return FResult(ERR_ERROR, "modify network filter error");
                }
            }
        }
    }

    std::string s_new_vnc_port;
    JSON_PARSE_STRING(doc, "new_vnc_port", s_new_vnc_port);
    if (!s_new_vnc_port.empty()) {
        uint16_t new_vnc_port = atoi(s_new_vnc_port.c_str());
        if (new_vnc_port < 5900 || new_vnc_port > 65535) {
            return FResult(ERR_ERROR,
                           "new_vnc_port " + s_new_vnc_port +
                               " is invalid! (usage: 5900 =< port <= 65535)");
        }

        fret = check_port_conflict(new_vnc_port, task_id);
        if (fret.errcode != ERR_SUCCESS) {
            return FResult(ERR_ERROR, "new_vnc_port " + s_new_vnc_port +
                                          " has been used!");
        }

        if (new_vnc_port != taskinfoPtr->getVncPort()) {
            need_reboot_vm = true;
            need_redefine_vm = true;
            TASK_LOG_INFO(task_id, "modify task old vnc port: "
                                       << taskinfoPtr->getVncPort()
                                       << ", new vnc port: " << new_vnc_port);
            taskinfoPtr->setVncPort(new_vnc_port);
        }
    }

    std::string new_gpu_count;
    std::map<std::string, std::list<std::string>> new_gpus;
    JSON_PARSE_STRING(doc, "new_gpu_count", new_gpu_count);
    if (!new_gpu_count.empty()) {
        if (!rent_all_machine)
            return FResult(ERR_ERROR, "can not modify gpu count");
        int count = (uint16_t)atoi(new_gpu_count.c_str());
        if (count < 0) return FResult(ERR_ERROR, "new gpu count is invalid");

        int old_gpu_count = TaskGpuMgr::instance().getTaskGpusCount(task_id);
        if (count != old_gpu_count) {
            if (allocate_gpu(count, new_gpus, order_gpu_index, task_id)) {
                std::map<std::string, std::shared_ptr<GpuInfo>> mp_gpus;
                for (auto& iter_gpu : new_gpus) {
                    std::shared_ptr<GpuInfo> ptr = std::make_shared<GpuInfo>();
                    ptr->setId(iter_gpu.first);
                    for (auto& iter_device_id : iter_gpu.second) {
                        ptr->addDeviceId(iter_device_id);
                    }
                    mp_gpus.insert({ptr->getId(), ptr});
                }

                TaskGpuMgr::instance().setTaskGpus(task_id, mp_gpus);
                TASK_LOG_INFO(
                    task_id, "modify task new gpu count: " << new_gpu_count
                                                           << ", old gpu count "
                                                           << old_gpu_count);
                need_reboot_vm = true;
                need_redefine_vm = true;
            } else {
                return FResult(ERR_ERROR, "allocate gpu failed");
            }
        }
    }

    std::string s_new_cpu_cores;
    JSON_PARSE_STRING(doc, "new_cpu_cores", s_new_cpu_cores);
    if (!s_new_cpu_cores.empty()) {
        if (!rent_all_machine)
            return FResult(ERR_ERROR, "can not modify cpu cores");
        int32_t new_cpu_cores = atoi(s_new_cpu_cores);
        int32_t cpu_sockets = 1, cpu_cores = 1, cpu_threads = 2;
        if (allocate_cpu(new_cpu_cores, cpu_sockets, cpu_cores, cpu_threads,
                         task_id, percent)) {
            if (cpu_sockets != taskinfoPtr->getCpuSockets() ||
                cpu_cores != taskinfoPtr->getCpuCoresPerSocket() ||
                cpu_threads != taskinfoPtr->getCpuThreadsPerCore()) {
                need_reboot_vm = true;
                need_redefine_vm = true;
                taskinfoPtr->setCpuSockets(cpu_sockets);
                taskinfoPtr->setCpuCoresPerSocket(cpu_cores);
                taskinfoPtr->setCpuThreadsPerCore(cpu_threads);
                std::vector<unsigned int> cpuset;
                if (VmClient::instance().GetCpuTune(task_id, cpuset) > 0) {
                    CpuTuneManager::instance().UnuseCpus(cpuset);
                }
                TASK_LOG_INFO(task_id,
                              "modify task new cpu cores: "
                                  << s_new_cpu_cores
                                  << ", sockets: " << cpu_sockets
                                  << ", cores per socket: " << cpu_cores
                                  << ", threads per core: " << cpu_threads);
            }
        } else {
            return FResult(ERR_ERROR, "allocate cpu failed");
        }
    }

    std::string new_mem_size;
    JSON_PARSE_STRING(doc, "new_mem_size", new_mem_size);
    if (!new_mem_size.empty()) {
        if (!rent_all_machine)
            return FResult(ERR_ERROR, "can not modify mem size");
        int64_t ksize = atoi(new_mem_size.c_str()) * 1024L * 1024L;
        if (allocate_mem(ksize, task_id, percent)) {
            if (ksize != taskinfoPtr->getMemSize()) {
                need_reboot_vm = true;
                need_redefine_vm = true;
                TASK_LOG_INFO(task_id,
                              "modify task old mem size: "
                                  << taskinfoPtr->getMemSize()
                                  << ", new mem size: " << new_mem_size);
                taskinfoPtr->setMemSize(ksize);
            }
        } else {
            run_shell("echo 3 > /proc/sys/vm/drop_caches");

            if (allocate_mem(ksize, task_id)) {
                if (ksize != taskinfoPtr->getMemSize()) {
                    need_reboot_vm = true;
                    need_redefine_vm = true;
                    TASK_LOG_INFO(task_id,
                                  "modify task old mem size: "
                                      << taskinfoPtr->getMemSize()
                                      << ", new mem size: " << new_mem_size);
                    taskinfoPtr->setMemSize(ksize);
                }
            } else {
                return FResult(ERR_ERROR, "allocate memory failed");
            }
        }
    }

    // being to redefine domain
    if (need_redefine_vm) {
        fret = VmClient::instance().RedefineDomain(taskinfoPtr);
        if (fret.errcode != ERR_SUCCESS) {
            TASK_LOG_ERROR(task_id, "modify task error: " << fret.errmsg);
            return fret;
        }
        TaskInfoMgr::instance().update(taskinfoPtr);
        std::vector<unsigned int> cpuset;
        if (VmClient::instance().GetCpuTune(task_id, cpuset) > 0) {
            CpuTuneManager::instance().UseCpus(cpuset);
        }
        TASK_LOG_INFO(task_id, "redefine task sucessful");
    }

    TASK_LOG_INFO(task_id, "modify task successful");
    if (need_reboot_vm)
        return FResult(ERR_SUCCESS,
                       "some changes will take effect on the next boot");
    return FResultOk;
}

// 设置task的用户名和密码
FResult TaskManager::passwdTask(
    const std::string& wallet,
    const std::shared_ptr<dbc::node_passwd_task_req_data>& data) {
    std::string task_id = data->task_id;
    auto taskinfoPtr = TaskInfoManager::instance().getTaskInfo(task_id);
    if (taskinfoPtr == nullptr) {
        return FResult(ERR_ERROR, "task not exist");
    }

    if (!VmClient::instance().IsExistDomain(task_id)) {
        return FResult(ERR_ERROR, "task not exist");
    }

    virDomainState vm_status = VmClient::instance().GetDomainStatus(task_id);
    if (vm_status != VIR_DOMAIN_RUNNING) {
        return FResult(ERR_ERROR,
                       "Only the running task can change the password");
    }

    rapidjson::Document doc;
    doc.Parse(data->additional.c_str());
    if (!doc.IsObject()) {
        return FResult(ERR_ERROR, "additional parse failed");
    }

    std::string username, password;
    JSON_PARSE_STRING(doc, "username", username);
    JSON_PARSE_STRING(doc, "password", password);
    if (password.empty())
        return FResult(ERR_ERROR, "password can not be empty");
    if (username.empty())
        return FResult(ERR_ERROR, "user name can not be empty");
    FResult fret = VmClient::instance().SetDomainUserPassword(task_id, username,
                                                              password, 1);
    if (fret.errcode == ERR_SUCCESS) {
        taskinfoPtr->setLoginPassword(password);
        std::string old_username = taskinfoPtr->getLoginUsername();
        if (old_username.empty() || old_username == "N/A")
            taskinfoPtr->setLoginUsername(username);
        TaskInfoMgr::instance().update(taskinfoPtr);
    }
    return fret;
}

// 获取task日志
FResult TaskManager::getTaskLog(const std::string& task_id,
                                QUERY_LOG_DIRECTION direction, int32_t nlines,
                                std::string& log_content) {
    auto taskinfo = TaskInfoMgr::instance().getTaskInfo(task_id);
    if (taskinfo == nullptr) {
        log_content = "";
        return FResult(ERR_ERROR, "task not exist");
    }

    if (!VmClient::instance().IsExistDomain(task_id)) {
        return FResult(ERR_ERROR, "task not exist");
    }

    return VmClient::instance().GetDomainLog(task_id, direction, nlines,
                                             log_content);
}

FResult TaskManager::listImages(
    const std::shared_ptr<dbc::node_list_images_req_data>& data,
    const AuthoriseResult& result, std::vector<ImageFile>& images) {
    try {
        ImageServer imgsvr;
        imgsvr.from_string(data->image_server);

        if (!result.success) {
            ImageMgr::instance().listLocalShareImages(imgsvr, images);
        } else {
            ImageMgr::instance().listWalletLocalShareImages(result.rent_wallet,
                                                            imgsvr, images);
        }

        return FResultOk;
    } catch (std::exception& e) {
        return FResultOk;
    }
}

FResult TaskManager::downloadImage(
    const std::string& wallet,
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

    std::vector<ImageFile> images;
    ImageServer imgsvr;
    imgsvr.from_string(data->image_server);
    ImageManager::instance().listShareImages(imgsvr, images);
    auto iter = std::find_if(images.begin(), images.end(),
                             [image_filename](const ImageFile& iter) {
                                 return iter.name == image_filename;
                             });
    if (iter == images.end()) {
        return FResult(ERR_ERROR, "image:" + image_filename + " not exist");
    }

    if (ImageManager::instance().isDownloading(image_filename)) {
        return FResult(ERR_ERROR,
                       "image:" + image_filename + " in downloading");
    }

    FResult fret =
        ImageManager::instance().download(image_filename, local_dir, imgsvr);
    if (fret.errcode != ERR_SUCCESS)
        return fret;
    else
        return FResultOk;
}

FResult TaskManager::downloadImageProgress(
    const std::string& wallet,
    const std::shared_ptr<dbc::node_download_image_progress_req_data>& data) {
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

    float progress = ImageManager::instance().downloadProgress(image_filename);

    std::string rsp_json = "{";
    rsp_json += "\"errcode\":0";
    rsp_json += ",\"message\":";
    rsp_json += "{";
    if (progress < 1.0f) {
        rsp_json += "\"status\": \"downloading\"";
        rsp_json +=
            ",\"progress\": \"" + std::to_string(int(progress * 100)) + "%\"";
    } else {
        rsp_json += "\"status\": \"done\"";
        rsp_json += ",\"progress\": \"100%\"";
    }
    rsp_json += "}";
    rsp_json += "}";

    return FResult(ERR_SUCCESS, rsp_json);
}

FResult TaskManager::stopDownloadImage(
    const std::string& wallet,
    const std::shared_ptr<dbc::node_stop_download_image_req_data>& data) {
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

    ImageManager::instance().terminateDownload(image_filename);

    std::string rsp_json = "{";
    rsp_json += "\"errcode\":0";
    rsp_json += ",\"message\":\"ok\"";
    rsp_json += "}";

    return FResult(ERR_SUCCESS, rsp_json);
}

FResult TaskManager::uploadImage(
    const std::string& wallet,
    const std::shared_ptr<dbc::node_upload_image_req_data>& data) {
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

    if (ImageManager::instance().isUploading(image_fullpath)) {
        return FResult(ERR_ERROR, "image:" + image_filename + " in uploading");
    }

    ImageServer imgsvr;
    imgsvr.from_string(data->image_server);
    FResult fret = ImageManager::instance().upload(image_fullpath, imgsvr);
    if (fret.errcode != ERR_SUCCESS)
        return fret;
    else
        return FResultOk;
}

FResult TaskManager::uploadImageProgress(
    const std::string& wallet,
    const std::shared_ptr<dbc::node_upload_image_progress_req_data>& data) {
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

    float progress = ImageManager::instance().uploadProgress(image_filename);

    std::string rsp_json = "{";
    rsp_json += "\"errcode\":0";
    rsp_json += ",\"message\":";
    rsp_json += "{";
    if (progress < 1.0f) {
        rsp_json += "\"status\": \"uploading\"";
        rsp_json +=
            ",\"progress\": \"" + std::to_string(int(progress * 100)) + "%\"";
    } else {
        rsp_json += "\"status\": \"done\"";
        rsp_json += ",\"progress\": \"100%\"";
    }
    rsp_json += "}";
    rsp_json += "}";

    return FResult(ERR_SUCCESS, rsp_json);
}

FResult TaskManager::stopUploadImage(
    const std::string& wallet,
    const std::shared_ptr<dbc::node_stop_upload_image_req_data>& data) {
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

    ImageManager::instance().terminateUpload(image_filename);

    std::string rsp_json = "{";
    rsp_json += "\"errcode\":0";
    rsp_json += ",\"message\":\"ok\"";
    rsp_json += "}";

    return FResult(ERR_SUCCESS, rsp_json);
}

FResult TaskManager::deleteImage(
    const std::string& wallet,
    const std::shared_ptr<dbc::node_delete_image_req_data>& data) {
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

    std::string image_fullpath = "/data/" + image_filename;

    if (!boost::filesystem::exists(image_fullpath)) {
        return FResult(ERR_ERROR,
                       "image file:" + image_filename + " is not exist");
    }

    if (ImageManager::instance().isDownloading(image_filename) ||
        ImageManager::instance().isUploading(image_fullpath)) {
        return FResult(ERR_ERROR,
                       "image file is in downloading or uploading, please try "
                       "again later");
    }

    ImageManager::instance().deleteImage(image_fullpath);

    return FResultOk;
}

void TaskManager::listAllTask(const std::string& wallet,
                              std::vector<std::shared_ptr<TaskInfo>>& vec) {
    auto renttask = WalletRentTaskMgr::instance().getWalletRentTask(wallet);
    if (renttask != nullptr) {
        std::vector<std::string> ids = renttask->getTaskIds();
        for (auto& it_id : ids) {
            auto taskinfo = TaskInfoMgr::instance().getTaskInfo(it_id);
            if (taskinfo != nullptr) {
                vec.push_back(taskinfo);
            }
        }
    }
}

void TaskManager::listRunningTask(const std::string& wallet,
                                  std::vector<std::shared_ptr<TaskInfo>>& vec) {
    auto renttask = WalletRentTaskMgr::instance().getWalletRentTask(wallet);
    if (renttask != nullptr) {
        std::vector<std::string> ids = renttask->getTaskIds();
        for (auto& it_id : ids) {
            auto taskinfo = TaskInfoMgr::instance().getTaskInfo(it_id);
            if (taskinfo != nullptr &&
                taskinfo->getTaskStatus() == TaskStatus::TS_Task_Running) {
                vec.push_back(taskinfo);
            }
        }
    }
}

int32_t TaskManager::getRunningTasksSize(const std::string& wallet) {
    int32_t count = 0;
    auto renttask = WalletRentTaskMgr::instance().getWalletRentTask(wallet);
    std::vector<std::string> ids = renttask->getTaskIds();
    for (auto& it_id : ids) {
        auto taskinfo = TaskInfoMgr::instance().getTaskInfo(it_id);
        if (taskinfo != nullptr &&
            taskinfo->getTaskStatus() == TaskStatus::TS_Task_Running) {
            count += 1;
        }
    }

    return count;
}

TaskStatus TaskManager::queryTaskStatus(const std::string& task_id) {
    auto taskinfo = TaskInfoMgr::instance().getTaskInfo(task_id);
    if (taskinfo == nullptr) {
        return TaskStatus::TS_Task_Shutoff;
    } else {
        TaskStatus task_status = taskinfo->getTaskStatus();
        virDomainState domain_status =
            VmClient::instance().GetDomainStatus(task_id);

        if ((task_status == TaskStatus::TS_Task_Shutoff &&
             domain_status != VIR_DOMAIN_SHUTOFF) ||
            (task_status == TaskStatus::TS_Task_Running &&
             domain_status != VIR_DOMAIN_RUNNING) ||
            (task_status == TaskStatus::TS_Task_PMSuspend &&
             domain_status != VIR_DOMAIN_PMSUSPENDED)) {
            if (domain_status == VIR_DOMAIN_SHUTOFF ||
                domain_status == VIR_DOMAIN_SHUTDOWN) {
                task_status = TaskStatus::TS_Task_Shutoff;
            } else if (domain_status == VIR_DOMAIN_RUNNING ||
                       domain_status == VIR_DOMAIN_PAUSED ||
                       domain_status == VIR_DOMAIN_BLOCKED) {
                task_status = TaskStatus::TS_Task_Running;
            } else if (domain_status == VIR_DOMAIN_PMSUSPENDED) {
                task_status = TaskStatus::TS_Task_PMSuspend;
            }

            taskinfo->setTaskStatus(task_status);
        }

        return task_status;
    }
}

int32_t TaskManager::getTaskAgentInterfaceAddress(
    const std::string& task_id,
    std::vector<std::tuple<std::string, std::string>>& address) {
    virDomainState vm_status = VmClient::instance().GetDomainStatus(task_id);
    if (vm_status != VIR_DOMAIN_RUNNING) return -1;
    std::vector<dbc::virDomainInterface> difaces;
    if (VmClient::instance().GetDomainInterfaceAddress(
            task_id, difaces, VIR_DOMAIN_INTERFACE_ADDRESSES_SRC_AGENT) > 0) {
        for (const auto& diface : difaces) {
            for (const auto& addr : diface.addrs) {
                if (addr.type == 0 &&
                    strncmp(addr.addr.c_str(), "127.", 4) != 0 &&
                    strncmp(addr.addr.c_str(), "192.168.122.", 12) != 0) {
                    // LOG_INFO << diface.name << " " << diface.hwaddr << " " <<
                    // addr.type << " " << addr.addr << " " << addr.prefix;
                    address.push_back(
                        std::make_tuple(diface.hwaddr, addr.addr));
                }
            }
        }
    }
    return address.size();
}

void TaskManager::deleteAllCheckTasks() {
    std::vector<std::string> check_ids;
    std::vector<std::string> ids = TaskInfoMgr::instance().getAllTaskIds();
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
    auto renttasks = WalletRentTaskMgr::instance().getAllWalletRentTasks();
    std::vector<std::string> check_ids;
    for (auto& it : renttasks) {
        if (it.first != wallet) {
            auto ids = it.second->getTaskIds();
            for (auto& id : ids) {
                auto taskinfo = TaskInfoMgr::instance().getTaskInfo(id);
                if (taskinfo != nullptr) {
                    if (id.find("vm_check_") != std::string::npos) {
                        check_ids.push_back(id);
                    }
                }
            }
        }
    }

    for (auto& task_id : check_ids) {
        TASK_LOG_INFO(task_id, "delete check task");
        delete_task(task_id);
    }
}

void TaskManager::closeOtherRentedTasks(const std::string& wallet) {
    std::vector<std::string> ids_running;
    auto renttasks = WalletRentTaskMgr::instance().getAllWalletRentTasks();
    for (const auto& iter : renttasks) {
        if (iter.first != wallet) {
            auto ids = iter.second->getTaskIds();
            for (const auto& task_id : ids) {
                if (VmClient::instance().GetDomainStatus(task_id) ==
                    VIR_DOMAIN_RUNNING) {
                    TASK_LOG_INFO(
                        task_id,
                        "task will be shutdown later because the owner is "
                            << iter.first << ", but current renter is "
                            << wallet);
                    ids_running.push_back(task_id);
                }
            }
        }
    }

    for (const auto& task_id : ids_running) {
        close_task(task_id);
    }
}

void TaskManager::checkRunningTasksRentStatus() {
    auto alltasks = TaskInfoMgr::instance().getAllTaskInfos();
    for (const auto& iter : alltasks) {
        auto taskinfo = iter.second;
        if (VmClient::instance().GetDomainStatus(iter.first) ==
            VIR_DOMAIN_RUNNING) {
            auto status = RentOrderManager::instance().GetRentStatus(
                taskinfo->getOrderId(), taskinfo->getRenterWallet());
            if (status != RentOrder::RentStatus::Renting) {
                TASK_LOG_INFO(
                    iter.first,
                    "task will be shutdown later because the rent status is "
                        << status << ", and rent order "
                        << taskinfo->getOrderId() << ", wallet "
                        << taskinfo->getRenterWallet());
                close_task(iter.first);
            }
        }
    }
}

FResult TaskManager::listTaskSnapshot(
    const std::string& wallet, const std::string& task_id,
    std::vector<dbc::snapshot_info>& snapshots) {
    auto taskinfo = TaskInfoMgr::instance().getTaskInfo(task_id);
    if (taskinfo == nullptr) {
        return FResult(ERR_ERROR, "task not exist");
    }

    if (!VmClient::instance().IsExistDomain(task_id)) {
        return FResult(ERR_ERROR, "task not exist");
    }

    std::string bios_mode = taskinfo->getBiosMode();
    if (bios_mode == "pxe")
        return FResult(ERR_ERROR, "pxe not support snapshot");

    TaskDiskMgr::instance().listSnapshots(task_id, snapshots);
    return FResultOk;
}

FResult TaskManager::createSnapshot(const std::string& wallet,
                                    const std::string& task_id,
                                    const std::string& snapshot_name,
                                    const ImageServer& imgsvr,
                                    const std::string& desc) {
    auto taskinfo = TaskInfoMgr::instance().getTaskInfo(task_id);
    if (taskinfo == nullptr) {
        return FResult(ERR_ERROR, "task not exist");
    }

    if (!VmClient::instance().IsExistDomain(task_id)) {
        return FResult(ERR_ERROR, "task not exist");
    }

    std::string bios_mode = taskinfo->getBiosMode();
    if (bios_mode == "pxe")
        return FResult(ERR_ERROR, "pxe not support snapshot");

    FResult fret = TaskDiskMgr::instance().createAndUploadSnapshot(
        task_id, snapshot_name, imgsvr, desc);
    return fret;
}

FResult TaskManager::terminateSnapshot(const std::string& wallet,
                                       const std::string& task_id,
                                       const std::string& snapshot_name) {
    auto taskinfo = TaskInfoMgr::instance().getTaskInfo(task_id);
    if (taskinfo == nullptr) {
        return FResult(ERR_ERROR, "task not exist");
    }

    if (!VmClient::instance().IsExistDomain(task_id)) {
        return FResult(ERR_ERROR, "task not exist");
    }

    std::string bios_mode = taskinfo->getBiosMode();
    if (bios_mode == "pxe")
        return FResult(ERR_ERROR, "pxe not support snapshot");

    TaskDiskMgr::instance().terminateUploadSnapshot(task_id, snapshot_name);
    return FResultOk;
}

void TaskManager::pushTaskEvent(const std::shared_ptr<TaskEvent>& ev) {
    std::unique_lock<std::mutex> lock(m_process_mtx);
    m_events.push(ev);
    m_process_cond.notify_one();
}

void TaskManager::process_task_thread_func() {
    while (m_running) {
        std::shared_ptr<TaskEvent> ev;
        {
            std::unique_lock<std::mutex> lock(m_process_mtx);
            m_process_cond.wait(
                lock, [this] { return !m_running || !m_events.empty(); });

            if (!m_running) break;

            ev = m_events.front();
            m_events.pop();
        }

        process_task(ev);
    }
}

void TaskManager::process_task(const std::shared_ptr<TaskEvent>& ev) {
    int32_t res_code = ERR_ERROR;
    std::string res_msg;
    switch (ev->type) {
        case TaskEventType::TET_CreateTask:
            process_create_task(ev);
            break;
        case TaskEventType::TET_StartTask:
            process_start_task(ev);
            break;
        case TaskEventType::TET_ShutdownTask:
            process_shutdown_task(ev);
            break;
        case TaskEventType::TET_PowerOffTask:
            process_poweroff_task(ev);
            break;
        case TaskEventType::TET_ReStartTask:
            process_restart_task(ev);
            break;
        case TaskEventType::TET_ForceRebootTask:
            process_force_reboot_task(ev);
            break;
        case TaskEventType::TET_ResetTask:
            process_reset_task(ev);
            break;
        case TaskEventType::TET_DeleteTask:
            process_delete_task(ev);
            break;
        case TaskEventType::TET_ResizeDisk:
            process_resize_disk(ev);
            break;
        case TaskEventType::TET_AddDisk:
            process_add_disk(ev);
            break;
        case TaskEventType::TET_DeleteDisk:
            process_delete_disk(ev);
            break;
        case TaskEventType::TET_CreateSnapshot:
            process_create_snapshot(ev);
            break;
        default:
            TASK_LOG_ERROR(ev->task_id, "unknown task event_type:" +
                                            std::to_string((int)ev->type));
            break;
    }

    TaskInfoMgr::instance().update_running_tasks();
}

void TaskManager::process_create_task(const std::shared_ptr<TaskEvent>& ev) {
    std::shared_ptr<CreateTaskEvent> ev_createtask =
        std::dynamic_pointer_cast<CreateTaskEvent>(ev);
    if (ev_createtask == nullptr) return;

    auto taskinfo = TaskInfoMgr::instance().getTaskInfo(ev->task_id);
    if (taskinfo == nullptr) return;

    ERRCODE ret = VmClient::instance().DefineNWFilter(ev->task_id,
                                                      taskinfo->getNwfilter());
    if (ret != ERR_SUCCESS) {
        if (taskinfo->getTaskStatus() == TaskStatus::TS_Task_Creating) {
            taskinfo->setTaskStatus(TaskStatus::TS_CreateTaskError);
        }
        TASK_LOG_ERROR(ev->task_id, "create network filter failed");
        return;
    }

    ret = VmClient::instance().CreateDomain(taskinfo);
    if (ret != ERR_SUCCESS) {
        if (taskinfo->getTaskStatus() == TaskStatus::TS_Task_Creating) {
            taskinfo->setTaskStatus(TaskStatus::TS_CreateTaskError);
        }
        TASK_LOG_ERROR(ev->task_id, "create domain failed");
        return;
    }

    if (ev_createtask->bios_mode != "pxe") {
        std::string local_ip =
            VmClient::instance().GetDomainLocalIP(ev->task_id);
        if (local_ip.empty()) {
            VmClient::instance().DestroyDomain(ev->task_id);

            if (taskinfo->getTaskStatus() == TaskStatus::TS_Task_Creating) {
                taskinfo->setTaskStatus(TaskStatus::TS_CreateTaskError);
            }

            TASK_LOG_ERROR(ev->task_id, "get domain local ip failed");
            TASK_LOG_ERROR(ev->task_id, "create task failed");
            return;
        }

        if (!create_task_iptable(
                ev->task_id, taskinfo->getSSHPort(), taskinfo->getRDPPort(),
                taskinfo->getCustomPort(), local_ip, taskinfo->getPublicIP())) {
            VmClient::instance().DestroyDomain(ev->task_id);

            if (taskinfo->getTaskStatus() == TaskStatus::TS_Task_Creating) {
                taskinfo->setTaskStatus(TaskStatus::TS_CreateTaskError);
            }
            TASK_LOG_ERROR(ev->task_id, "create task iptable failed");
            TASK_LOG_ERROR(ev->task_id, "create task failed");
            return;
        }

        add_iptable_to_system(ev->task_id);

        std::string login_username = isLinuxOS(taskinfo->getOperationSystem())
                                         ? g_vm_ubuntu_login_username
                                         : g_vm_windows_login_username;
        FResult fret = VmClient::instance().SetDomainUserPassword(
            ev->task_id, login_username, taskinfo->getLoginPassword());
        if (fret.errcode != ERR_SUCCESS) {
            taskinfo->setLoginPassword("N/A");
            taskinfo->setLoginUsername("N/A");
            // VmClient::instance().DestroyDomain(ev->task_id);

            // if (taskinfo->getTaskStatus() == TaskStatus::TS_Task_Creating) {
            //     taskinfo->setTaskStatus(TaskStatus::TS_CreateTaskError);
            // }
            // TASK_LOG_ERROR(ev->task_id, "set domain password failed");
            // return;
        } else {
            taskinfo->setLoginUsername(login_username);
        }
    }

    std::vector<unsigned int> cpuset;
    if (VmClient::instance().GetCpuTune(ev->task_id, cpuset) > 0) {
        CpuTuneManager::instance().UseCpus(cpuset);
    }

    if (taskinfo->getTaskStatus() == TaskStatus::TS_Task_Creating) {
        taskinfo->setTaskStatus(TaskStatus::TS_Task_Running);
    }
    TASK_LOG_INFO(ev->task_id, "create task successful");
}

bool TaskManager::create_task_iptable(
    const std::string& task_id, uint16_t ssh_port, uint16_t rdp_port,
    const std::vector<std::string>& custom_port,
    const std::string& task_local_ip, const std::string& public_ip) {
    std::string host_ip = SystemInfo::instance().GetDefaultRouteIp();
    if (!host_ip.empty() && !task_local_ip.empty()) {
        // shell_add_iptable_to_system(task_id, host_ip, ssh_port, rdp_port,
        // custom_port, task_local_ip, public_ip);

        std::shared_ptr<IptableInfo> iptable = std::make_shared<IptableInfo>();
        iptable->setTaskId(task_id);
        iptable->setHostIP(host_ip);
        iptable->setTaskLocalIP(task_local_ip);
        iptable->setSSHPort(ssh_port);
        iptable->setRDPPort(rdp_port);
        iptable->setCustomPort(custom_port);
        iptable->setPublicIP(public_ip);
        TaskIptableMgr::instance().add(iptable);

        TASK_LOG_INFO(
            task_id,
            "create iptable successful,"
            " host_ip:" +
                host_ip + " ssh_port:" + std::to_string(ssh_port) +
                " rdp_port:" + std::to_string(rdp_port) +
                " custom_port.size:" + std::to_string(custom_port.size()) +
                " task_local_ip:" + task_local_ip + " public_ip:" + public_ip);
        return true;
    } else {
        TASK_LOG_ERROR(
            task_id,
            "create iptable failed, host_ip or task_local_ip is empty: "
                << "host_ip:" << host_ip
                << ", task_local_ip:" << task_local_ip);
        return false;
    }
}

void TaskManager::getNeededBackingImage(
    const std::string& image_name, std::vector<std::string>& backing_images) {
    std::string cur_image = "/data/" + image_name;
    while (cur_image.find("/data/") != std::string::npos) {
        if (!boost::filesystem::exists(cur_image)) {
            backing_images.push_back(cur_image);
            break;
        }

        std::string cmd_get_backing_file =
            "qemu-img info " + cur_image +
            " | grep -i 'backing file:' | awk -F ': ' '{print $2}'";
        cur_image = run_shell(cmd_get_backing_file);
    }
}

void TaskManager::update_task_iptable(const std::string& domain_name) {
    auto taskinfo = TaskInfoMgr::instance().getTaskInfo(domain_name);
    if (taskinfo == nullptr) return;

    if (taskinfo->getBiosMode() == "pxe") return;

    if (taskinfo->getTaskStatus() != TaskStatus::TS_Task_Running) return;

    auto taskIptablePtr =
        TaskIptableMgr::instance().getIptableInfo(domain_name);
    if (taskIptablePtr == nullptr) return;

    std::string local_ip = VmClient::instance().GetDomainLocalIP(domain_name);
    if (!local_ip.empty() && taskIptablePtr->getTaskLocalIP() != local_ip) {
        TASK_LOG_INFO(domain_name, "local ip changed from "
                                       << taskIptablePtr->getTaskLocalIP()
                                       << " to " << local_ip);
        taskIptablePtr->setTaskLocalIP(local_ip);
        TaskIptableMgr::instance().update(taskIptablePtr);
    }
}

void TaskManager::process_start_task(const std::shared_ptr<TaskEvent>& ev) {
    std::shared_ptr<StartTaskEvent> ev_starttask =
        std::dynamic_pointer_cast<StartTaskEvent>(ev);
    if (ev_starttask == nullptr) return;

    auto taskinfo = TaskInfoMgr::instance().getTaskInfo(ev->task_id);
    if (taskinfo == nullptr) return;

    ERRCODE ret = VmClient::instance().StartDomain(ev->task_id);
    if (ret != ERR_SUCCESS) {
        if (taskinfo->getTaskStatus() == TaskStatus::TS_Task_Starting) {
            taskinfo->setTaskStatus(TaskStatus::TS_StartTaskError);
        }
        TASK_LOG_ERROR(ev->task_id, "start task failed");
    } else {
        // 创建失败导致没有写iptables，修改虚拟机后启动成功了可以抢救一下。
        if (taskinfo->getBiosMode() != "pxe" &&
            TaskIptableMgr::instance().getIptableInfo(ev->task_id) == nullptr) {
            std::string local_ip =
                VmClient::instance().GetDomainLocalIP(ev->task_id);
            if (local_ip.empty()) {
                VmClient::instance().DestroyDomain(ev->task_id);

                if (taskinfo->getTaskStatus() == TaskStatus::TS_Task_Starting) {
                    taskinfo->setTaskStatus(TaskStatus::TS_StartTaskError);
                }

                TASK_LOG_ERROR(ev->task_id, "get domain local ip failed");
                TASK_LOG_ERROR(ev->task_id, "start task failed");
                return;
            }

            if (!create_task_iptable(ev->task_id, taskinfo->getSSHPort(),
                                     taskinfo->getRDPPort(),
                                     taskinfo->getCustomPort(), local_ip,
                                     taskinfo->getPublicIP())) {
                VmClient::instance().DestroyDomain(ev->task_id);

                if (taskinfo->getTaskStatus() == TaskStatus::TS_Task_Starting) {
                    taskinfo->setTaskStatus(TaskStatus::TS_StartTaskError);
                }
                TASK_LOG_ERROR(ev->task_id, "create task iptable failed");
                TASK_LOG_ERROR(ev->task_id, "start task failed");
                return;
            }
        }
        if (taskinfo->getTaskStatus() == TaskStatus::TS_Task_Starting) {
            taskinfo->setTaskStatus(TaskStatus::TS_Task_Running);
        }
        update_task_iptable(ev->task_id);
        add_iptable_to_system(ev->task_id);

        std::vector<unsigned int> cpuset;
        if (VmClient::instance().GetCpuTune(ev->task_id, cpuset) > 0) {
            CpuTuneManager::instance().UseCpus(cpuset);
        }

        TASK_LOG_INFO(ev->task_id, "start task successful");
    }
}

void TaskManager::process_shutdown_task(const std::shared_ptr<TaskEvent>& ev) {
    std::shared_ptr<ShutdownTaskEvent> ev_shutdowntask =
        std::dynamic_pointer_cast<ShutdownTaskEvent>(ev);
    if (ev_shutdowntask == nullptr) return;

    auto taskinfo = TaskInfoMgr::instance().getTaskInfo(ev->task_id);
    if (taskinfo == nullptr) return;

    ERRCODE ret = VmClient::instance().DestroyDomain(ev->task_id);
    if (ret != ERR_SUCCESS) {
        if (taskinfo->getTaskStatus() == TaskStatus::TS_Task_BeingShutdown) {
            taskinfo->setTaskStatus(TaskStatus::TS_ShutdownTaskError);
        }
        TASK_LOG_ERROR(ev->task_id, "shutdown task failed");
    } else {
        if (taskinfo->getTaskStatus() == TaskStatus::TS_Task_BeingShutdown) {
            taskinfo->setTaskStatus(TaskStatus::TS_Task_Shutoff);
        }
        remove_iptable_from_system(ev->task_id);
        TASK_LOG_INFO(ev->task_id, "shutdown task successful");

        std::vector<unsigned int> cpuset;
        if (VmClient::instance().GetCpuTune(ev->task_id, cpuset) > 0) {
            CpuTuneManager::instance().UnuseCpus(cpuset);
        }

        std::string bios_mode = taskinfo->getBiosMode();
        if (bios_mode != "pxe") {
            // vda root backfile
            do {
                if (taskinfo->getVdaRootBackfile().empty()) {
                    std::string vda_backfile =
                        "/data/" + taskinfo->getImageName();
                    std::string cmd =
                        "qemu-img info " + vda_backfile +
                        " | grep -i 'backing file:' | awk -F ': ' '{print $2}'";
                    std::string back_file = run_shell(cmd);
                    std::string root_backfile = vda_backfile;
                    while (!back_file.empty()) {
                        root_backfile = back_file;

                        cmd = "qemu-img info " + back_file +
                              " | grep -i 'backing file:' | awk -F ': ' "
                              "'{print $2}'";
                        back_file = run_shell(cmd);
                    }
                    taskinfo->setVdaRootBackfile(root_backfile);
                    TaskInfoMgr::instance().update(taskinfo);
                }
            } while (0);
        }
    }
}

void TaskManager::process_poweroff_task(const std::shared_ptr<TaskEvent>& ev) {
    std::shared_ptr<PowerOffTaskEvent> ev_powerofftask =
        std::dynamic_pointer_cast<PowerOffTaskEvent>(ev);
    if (ev_powerofftask == nullptr) return;

    auto taskinfo = TaskInfoMgr::instance().getTaskInfo(ev->task_id);
    if (taskinfo == nullptr) return;

    ERRCODE ret = VmClient::instance().DestroyDomain(ev->task_id);
    if (ret != ERR_SUCCESS) {
        if (taskinfo->getTaskStatus() == TaskStatus::TS_Task_BeingPoweroff) {
            taskinfo->setTaskStatus(TaskStatus::TS_PoweroffTaskError);
        }
        TASK_LOG_ERROR(ev->task_id, "poweroff task failed");
    } else {
        if (taskinfo->getTaskStatus() == TaskStatus::TS_Task_BeingPoweroff) {
            taskinfo->setTaskStatus(TaskStatus::TS_Task_Shutoff);
        }
        remove_iptable_from_system(ev->task_id);
        TASK_LOG_INFO(ev->task_id, "poweroff task successful");

        std::vector<unsigned int> cpuset;
        if (VmClient::instance().GetCpuTune(ev->task_id, cpuset) > 0) {
            CpuTuneManager::instance().UnuseCpus(cpuset);
        }

        std::string bios_mode = taskinfo->getBiosMode();
        if (bios_mode != "pxe") {
            // vda root backfile
            do {
                if (taskinfo->getVdaRootBackfile().empty()) {
                    std::string vda_backfile =
                        "/data/" + taskinfo->getImageName();
                    std::string cmd =
                        "qemu-img info " + vda_backfile +
                        " | grep -i 'backing file:' | awk -F ': ' '{print $2}'";
                    std::string back_file = run_shell(cmd);
                    std::string root_backfile = vda_backfile;
                    while (!back_file.empty()) {
                        root_backfile = back_file;

                        cmd = "qemu-img info " + back_file +
                              " | grep -i 'backing file:' | awk -F ': ' "
                              "'{print $2}'";
                        back_file = run_shell(cmd);
                    }
                    taskinfo->setVdaRootBackfile(root_backfile);
                    TaskInfoMgr::instance().update(taskinfo);
                }
            } while (0);
        }
    }
}

void TaskManager::process_restart_task(const std::shared_ptr<TaskEvent>& ev) {
    std::shared_ptr<ReStartTaskEvent> ev_restarttask =
        std::dynamic_pointer_cast<ReStartTaskEvent>(ev);
    if (ev_restarttask == nullptr) return;

    auto taskinfo = TaskInfoMgr::instance().getTaskInfo(ev->task_id);
    if (taskinfo == nullptr) return;

    virDomainState vm_status =
        VmClient::instance().GetDomainStatus(ev->task_id);
    if (vm_status == VIR_DOMAIN_SHUTOFF) {
        ERRCODE ret = VmClient::instance().StartDomain(ev->task_id);
        if (ret != ERR_SUCCESS) {
            if (taskinfo->getTaskStatus() == TaskStatus::TS_Task_Restarting) {
                taskinfo->setTaskStatus(TaskStatus::TS_RestartTaskError);
            }
            TASK_LOG_ERROR(ev->task_id, "restart task failed");
        } else {
            if (taskinfo->getTaskStatus() == TaskStatus::TS_Task_Restarting) {
                taskinfo->setTaskStatus(TaskStatus::TS_Task_Running);
            }
            update_task_iptable(ev->task_id);
            add_iptable_to_system(ev->task_id);
            std::vector<unsigned int> cpuset;
            if (VmClient::instance().GetCpuTune(ev->task_id, cpuset) > 0) {
                CpuTuneManager::instance().UseCpus(cpuset);
            }
            TASK_LOG_INFO(ev->task_id, "restart task successful");
        }
    } else if (vm_status == VIR_DOMAIN_RUNNING) {
        ERRCODE ret = VmClient::instance().RebootDomain(ev->task_id);
        if (ret != ERR_SUCCESS) {
            if (taskinfo->getTaskStatus() == TaskStatus::TS_Task_Restarting) {
                taskinfo->setTaskStatus(TaskStatus::TS_RestartTaskError);
            }
            TASK_LOG_ERROR(ev->task_id, "restart task failed");
        } else {
            if (taskinfo->getTaskStatus() == TaskStatus::TS_Task_Restarting) {
                taskinfo->setTaskStatus(TaskStatus::TS_Task_Running);
            }
            update_task_iptable(ev->task_id);
            add_iptable_to_system(ev->task_id);
            std::vector<unsigned int> cpuset;
            if (VmClient::instance().GetCpuTune(ev->task_id, cpuset) > 0) {
                CpuTuneManager::instance().UseCpus(cpuset);
            }
            TASK_LOG_INFO(ev->task_id, "restart task successful");
        }
    }
}

void TaskManager::process_force_reboot_task(
    const std::shared_ptr<TaskEvent>& ev) {
    std::shared_ptr<ForceRebootTaskEvent> ev_forcereboottask =
        std::dynamic_pointer_cast<ForceRebootTaskEvent>(ev);
    if (ev_forcereboottask == nullptr) return;

    auto taskinfo = TaskInfoMgr::instance().getTaskInfo(ev->task_id);
    if (taskinfo == nullptr) return;

    virDomainState vm_status =
        VmClient::instance().GetDomainStatus(ev->task_id);
    if (vm_status == VIR_DOMAIN_SHUTOFF) {
        ERRCODE ret = VmClient::instance().StartDomain(ev->task_id);
        if (ret != ERR_SUCCESS) {
            if (taskinfo->getTaskStatus() == TaskStatus::TS_Task_Restarting) {
                taskinfo->setTaskStatus(TaskStatus::TS_RestartTaskError);
            }
            TASK_LOG_ERROR(ev->task_id, "restart task failed");
        } else {
            if (taskinfo->getTaskStatus() == TaskStatus::TS_Task_Restarting) {
                taskinfo->setTaskStatus(TaskStatus::TS_Task_Running);
            }
            update_task_iptable(ev->task_id);
            add_iptable_to_system(ev->task_id);
            std::vector<unsigned int> cpuset;
            if (VmClient::instance().GetCpuTune(ev->task_id, cpuset) > 0) {
                CpuTuneManager::instance().UseCpus(cpuset);
            }
            TASK_LOG_INFO(ev->task_id, "restart task successful");
        }
    } else /*if (vm_status == VIR_DOMAIN_RUNNING)*/ {
        ERRCODE ret = VmClient::instance().DestroyDomain(ev->task_id);
        if (ret != ERR_SUCCESS) {
            if (taskinfo->getTaskStatus() == TaskStatus::TS_Task_Restarting) {
                taskinfo->setTaskStatus(TaskStatus::TS_RestartTaskError);
            }
            TASK_LOG_ERROR(ev->task_id, "restart task failed");
        } else {
            sleep(3);
            if ((ret = VmClient::instance().StartDomain(ev->task_id)) ==
                ERR_SUCCESS) {
                if (taskinfo->getTaskStatus() ==
                    TaskStatus::TS_Task_Restarting) {
                    taskinfo->setTaskStatus(TaskStatus::TS_Task_Running);
                }
                update_task_iptable(ev->task_id);
                add_iptable_to_system(ev->task_id);
                std::vector<unsigned int> cpuset;
                if (VmClient::instance().GetCpuTune(ev->task_id, cpuset) > 0) {
                    CpuTuneManager::instance().UseCpus(cpuset);
                }
                TASK_LOG_INFO(ev->task_id, "restart task successful");
            } else {
                if (taskinfo->getTaskStatus() ==
                    TaskStatus::TS_Task_Restarting) {
                    taskinfo->setTaskStatus(TaskStatus::TS_RestartTaskError);
                }
                TASK_LOG_ERROR(ev->task_id, "restart task failed");
            }
        }
    }
}

void TaskManager::process_reset_task(const std::shared_ptr<TaskEvent>& ev) {
    std::shared_ptr<ResetTaskEvent> ev_resettask =
        std::dynamic_pointer_cast<ResetTaskEvent>(ev);
    if (ev_resettask == nullptr) return;

    auto taskinfo = TaskInfoMgr::instance().getTaskInfo(ev->task_id);
    if (taskinfo == nullptr) return;

    ERRCODE ret = VmClient::instance().ResetDomain(ev->task_id);
    if (ret != ERR_SUCCESS) {
        if (taskinfo->getTaskStatus() == TaskStatus::TS_Task_Resetting) {
            taskinfo->setTaskStatus(TaskStatus::TS_ResetTaskError);
        }
        TASK_LOG_ERROR(ev->task_id, "reset task failed");
    } else {
        if (taskinfo->getTaskStatus() == TaskStatus::TS_Task_Resetting) {
            taskinfo->setTaskStatus(TaskStatus::TS_Task_Running);
        }
        TASK_LOG_INFO(ev->task_id, "reset task successful");
    }
}

void TaskManager::process_delete_task(const std::shared_ptr<TaskEvent>& ev) {
    auto taskinfo = TaskInfoMgr::instance().getTaskInfo(ev->task_id);
    if (taskinfo == nullptr) return;

    LOG_INFO << "process_delete: task_id=" << ev->task_id;

    delete_task(ev->task_id);
}

void TaskManager::process_resize_disk(const std::shared_ptr<TaskEvent>& ev) {
    TaskDiskMgr::instance().process_resize_disk(ev);
}

void TaskManager::process_add_disk(const std::shared_ptr<TaskEvent>& ev) {
    TaskDiskMgr::instance().process_add_disk(ev);
}

void TaskManager::process_delete_disk(const std::shared_ptr<TaskEvent>& ev) {
    TaskDiskMgr::instance().process_delete_disk(ev);
}

void TaskManager::process_create_snapshot(
    const std::shared_ptr<TaskEvent>& ev) {
    TaskDiskMgr::instance().process_create_snapshot(ev);
}

void TaskManager::prune_task_thread_func() {
    while (m_running) {
        static uint32_t timer_frequency =
            ConfManager::instance().GetCheckVmExpirationFrequency();
        std::unique_lock<std::mutex> lock(m_prune_mtx);
        m_prune_cond.wait_for(
            lock, std::chrono::seconds(timer_frequency),
            [this] { return !m_running || !m_events.empty(); });

        if (!m_running) break;

        LOG_INFO << "prune task thread timer start running";

        shell_remove_reject_iptable_from_system();

        HttpDBCChainClient::instance().update_chain_order(
            ConfManager::instance().GetDbcChainDomain());

        MACHINE_STATUS machine_status =
            HttpDBCChainClient::instance().request_machine_status(
                ConfManager::instance().GetNodeId());
        if (machine_status == MACHINE_STATUS::Unknown) continue;

        // 判断是否单卡租用，如果是，更新订单信息。
        std::set<std::string> orders;
        bool is_order = HttpDBCChainClient::instance().getRentOrderList(
            ConfManager::instance().GetNodeId(), orders);
        if (is_order && !orders.empty()) {
            for (const auto& order : orders) {
                RentOrderManager::instance().UpdateRentOrder(
                    ConfManager::instance().GetNodeId(), order);
            }
        }
        // RentOrderManager::instance().UpdateRentOrders(ConfManager::instance().GetNodeId());
        std::set<std::string> renting_orders =
            RentOrderManager::instance().GetRentingOrders();
        if (is_order && !renting_orders.empty()) {
            for (const auto& order : renting_orders) {
                if (orders.count(order) > 0) continue;
                RentOrderManager::instance().UpdateRentOrder(
                    ConfManager::instance().GetNodeId(), order);
            }
        }

        std::string cur_renter;
        int64_t cur_rent_end = 0;
        HttpDBCChainClient::instance().request_cur_renter(
            ConfManager::instance().GetNodeId(), cur_renter, cur_rent_end);
        int64_t cur_block = HttpDBCChainClient::instance().request_cur_block();

        // clear expired rent order
        RentOrderManager::instance().ClearExpiredRentOrder(cur_block);

        auto wallet_renttasks =
            WalletRentTaskMgr::instance().getAllWalletRentTasks();
        for (auto& it : wallet_renttasks) {
            if (machine_status == MACHINE_STATUS::Verify) {
                std::vector<std::string> ids = it.second->getTaskIds();
                for (auto& task_id : ids) {
                    if (task_id.find("vm_check_") == std::string::npos) {
                        TASK_LOG_INFO(task_id,
                                      "stop task and machine status is "
                                          << (int32_t)machine_status);
                        close_task(task_id);
                    }
                }
            } else if (machine_status == MACHINE_STATUS::Online) {
                std::vector<std::string> ids = it.second->getTaskIds();
                for (auto& task_id : ids) {
                    if (task_id.find("vm_check_") == std::string::npos) {
                        TASK_LOG_INFO(task_id,
                                      "stop task and machine status is "
                                          << (int32_t)machine_status);
                        close_task(task_id);
                    } else {
                        TASK_LOG_INFO(task_id,
                                      "delete check task and machine status is "
                                          << (int32_t)machine_status);
                        delete_task(task_id);
                    }
                }
            } else if (machine_status == MACHINE_STATUS::Rented) {
                std::vector<std::string> ids = it.second->getTaskIds();
                for (auto& task_id : ids) {
                    if (task_id.find("vm_check_") != std::string::npos) {
                        TASK_LOG_INFO(task_id,
                                      "delete check task and machine status is "
                                          << (int32_t)machine_status);
                        delete_task(task_id);
                    }
                }
            }

            if (machine_status == MACHINE_STATUS::Online ||
                machine_status == MACHINE_STATUS::Rented) {
                if (is_order) {
                    for (const auto& task_id : it.second->getTaskIds()) {
                        auto taskinfo =
                            TaskInfoMgr::instance().getTaskInfo(task_id);
                        if (!taskinfo) {
                            if (!VmClient::instance().IsExistDomain(task_id)) {
                                // delete_task(task_id);
                                // delete iptable
                                remove_iptable_from_system(task_id);
                                TaskIptableMgr::instance().del(task_id);
                                // delete renttask
                                WalletRentTaskMgr::instance().del(task_id);
                                // undefine network filter
                                VmClient::instance().UndefineNWFilter(task_id);
                                LOG_ERROR << task_id << " not existed";
                            } else {
                                LOG_ERROR
                                    << task_id
                                    << " could not get task info, please "
                                       "restart or contact with dbc developers";
                            }
                        } else if (taskinfo->getOrderId().empty()) {
                            TASK_LOG_INFO(
                                task_id,
                                "order id is empty while dbc chain need");
                            close_task(task_id);
                            int64_t wallet_rent_end = it.second->getRentEnd();
                            int64_t reserve_end =
                                wallet_rent_end + 120 * 24 * 10;  //保留10天
                            if (cur_block > 0 && reserve_end < cur_block) {
                                TASK_LOG_INFO(task_id,
                                              "order id is empty too long "
                                              "while dbc chain need");
                                delete_task(task_id);
                            }
                        } else {
                            uint64_t order_rent_end =
                                RentOrderManager::instance().GetRentEnd(
                                    taskinfo->getOrderId(), it.first);
                            if (order_rent_end > 0) {
                                if (cur_block > order_rent_end + 120 * 24 * 3) {
                                    delete_task(task_id);
                                    TASK_LOG_INFO(
                                        task_id,
                                        "stop task and machine status:"
                                            << (int32_t)machine_status
                                            << ", order id:"
                                            << taskinfo->getOrderId()
                                            << ", cur block:" << cur_block
                                            << ", task rent end:"
                                            << order_rent_end);
                                } else if (cur_block > order_rent_end) {
                                    close_task(task_id);
                                    TASK_LOG_INFO(
                                        task_id,
                                        "stop task and machine status:"
                                            << (int32_t)machine_status
                                            << ", order id:"
                                            << taskinfo->getOrderId()
                                            << ", cur block:" << cur_block
                                            << ", task rent end:"
                                            << order_rent_end);
                                }
                            }
                        }
                    }
                }
                if (it.first != cur_renter && !cur_renter.empty()) {
                    std::vector<std::string> ids = it.second->getTaskIds();
                    for (auto& task_id : ids) {
                        if (VIR_DOMAIN_SHUTOFF !=
                            VmClient::instance().GetDomainStatus(task_id)) {
                            TASK_LOG_INFO(
                                task_id,
                                "stop task and machine status: "
                                    << (int32_t)machine_status
                                    << ", cur_renter:" << cur_renter
                                    << ", cur_rent_end:" << cur_rent_end
                                    << ", cur_wallet:" << it.first);
                            close_task(task_id);
                        }
                        clear_task_public_ip(task_id);
                    }

                    // 1小时出120个块
                    int64_t wallet_rent_end = it.second->getRentEnd();
                    int64_t reserve_end =
                        wallet_rent_end + 120 * 24 * 10;  //保留10天
                    if (cur_block > 0 && wallet_rent_end > 0 &&
                        reserve_end < cur_block) {
                        ids = it.second->getTaskIds();
                        for (auto& task_id : ids) {
                            TASK_LOG_INFO(
                                task_id,
                                "delete task and machine status: "
                                    << (int32_t)machine_status
                                    << " ,task rent end: "
                                    << it.second->getRentEnd()
                                    << " ,current block:" << cur_block);
                            delete_task(task_id);
                        }
                    }
                } else if (it.first == cur_renter) {
                    if (cur_rent_end > it.second->getRentEnd()) {
                        int64_t old_rent_end = it.second->getRentEnd();
                        it.second->setRentEnd(cur_rent_end);
                        WalletRentTaskMgr::instance().update(it.second);
                        LOG_INFO << "update rent_end, wallet=" << it.first
                                 << ", update rent_end=old:" << old_rent_end
                                 << ",new:" << cur_rent_end;
                    }
                }
            }
        }

        if (is_order) {
            auto alltasks = TaskInfoMgr::instance().getAllTaskInfos();
            for (const auto& taskptr : alltasks) {
                if (taskptr.second->getOrderId().empty()) {
                    close_task(taskptr.first);
                    TASK_LOG_INFO(taskptr.first,
                                  "order id is empty while dbc chain need");
                }
            }
        }

        TaskInfoMgr::instance().update_running_tasks();
        TaskInfoMgr::instance().update_deleted_tasks();

        // 空闲状态定时清理缓存
        if (machine_status == MACHINE_STATUS::Online) {
            run_shell("echo 3 > /proc/sys/vm/drop_caches");
        }

        if (cur_rent_end <= 0 && orders.empty()) {
            broadcast_message("empty");
        } else {
            broadcast_message("renting");
        }
    }
}

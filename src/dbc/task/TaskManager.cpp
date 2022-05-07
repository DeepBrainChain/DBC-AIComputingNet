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
#include "detail/VxlanManager.h"
#include "service/peer_request_service/p2p_lan_service.h"

FResult TaskManager::init() {
    if (!VmClient::instance().init()) {
        return FResult(ERR_ERROR, "connect libvirt tcp service failed");
    }

    FResult fret = TaskInfoMgr::instance().init();
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
		return FResult(ERR_ERROR, "wallet_renttask manager init failed;");
	}

    fret = WalletSessionIDMgr::instance().init();
    if (fret.errcode != ERR_SUCCESS) {
        return FResult(ERR_ERROR, "wallet_sessionid manager init failed");
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
    setsockopt(m_udp_fd, SOL_SOCKET, SO_BROADCAST | SO_REUSEADDR, &optval, sizeof(int));

	m_running = true;
	if (m_process_thread == nullptr) {
		m_process_thread = new std::thread(&TaskManager::process_task_thread_func, this);
	}
	if (m_prune_thread == nullptr) {
		m_prune_thread = new std::thread(&TaskManager::prune_task_thread_func, this);
	}

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
}

void TaskManager::broadcast_message(const std::string& msg) {
    if (m_udp_fd == -1) {
        m_udp_fd = socket(PF_INET, SOCK_DGRAM, 0);
        int optval = 1;
        setsockopt(m_udp_fd, SOL_SOCKET, SO_BROADCAST | SO_REUSEADDR, &optval, sizeof(int));
    }

    if (m_udp_fd != -1) {
        // json by base64
        // { "node_id": "xxx", "status": "renting" }
        // { "node_id": "xxx", "status": "empty" }
        std::string strjson = "{\"node_id\":\"" + ConfManager::instance().GetNodeId() + "\", \"status\":\"" + msg + "\"}";
        std::string strbase64 = base64_encode((unsigned char*)strjson.c_str(), strjson.size());

        struct sockaddr_in theirAddr;
        memset(&theirAddr, 0, sizeof(struct sockaddr_in));
        theirAddr.sin_family = AF_INET;
        theirAddr.sin_addr.s_addr = inet_addr("255.255.255.255");
        theirAddr.sin_port = htons(55555);

        int sendBytes;
        if ((sendBytes = sendto(m_udp_fd, strbase64.c_str(), strbase64.size(), 0,
            (struct sockaddr*)&theirAddr, sizeof(struct sockaddr))) == -1) {
            LOG_ERROR << "udp broadcast fail, errno=" << errno;
        }
    }
}

FResult TaskManager::init_tasks_status() {
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
            virDomainState st = VmClient::instance().GetDomainStatus(iter.first);
            if (st == VIR_DOMAIN_SHUTOFF || st == VIR_DOMAIN_SHUTDOWN 
                || st == VIR_DOMAIN_CRASHED || st == VIR_DOMAIN_NOSTATE) {
                iter.second->setTaskStatus(TaskStatus::TS_Task_Shutoff);
            }
            else if (st == VIR_DOMAIN_RUNNING || st == VIR_DOMAIN_PAUSED) {
                iter.second->setTaskStatus(TaskStatus::TS_Task_Running);
            }
            else if (st == VIR_DOMAIN_PMSUSPENDED) {
                iter.second->setTaskStatus(TaskStatus::TS_Task_PMSuspend);
            }
        }
    }

    return FResultOk;
}

void TaskManager::start_task(const std::string &task_id) {
    virDomainState vm_status = VmClient::instance().GetDomainStatus(task_id);
    if (vm_status == VIR_DOMAIN_RUNNING) return;

    if (VmClient::instance().StartDomain(task_id) == ERR_SUCCESS) {
        auto taskinfo = TaskInfoMgr::instance().getTaskInfo(task_id);
        if (taskinfo != nullptr) {
            taskinfo->setTaskStatus(TaskStatus::TS_Task_Running);
        }
		add_iptable_to_system(task_id);
		TASK_LOG_INFO(task_id, "start task successful");
    }
}

void TaskManager::close_task(const std::string &task_id) {
    virDomainState vm_status = VmClient::instance().GetDomainStatus(task_id);
    if (vm_status == VIR_DOMAIN_SHUTOFF) return;

    if (VmClient::instance().DestroyDomain(task_id) == ERR_SUCCESS) {
        auto taskinfo = TaskInfoMgr::instance().getTaskInfo(task_id);
        if (taskinfo != nullptr) {
			taskinfo->setTaskStatus(TaskStatus::TS_Task_Shutoff);
        }
		remove_iptable_from_system(task_id);
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

        shell_remove_iptable_from_system(task_id, host_ip, ssh_port, task_local_ip);
    }
}

void TaskManager::add_iptable_to_system(const std::string &task_id) {
    auto iptableinfo = TaskIptableMgr::instance().getIptableInfo(task_id);
    if (iptableinfo != nullptr) {
        std::string host_ip = iptableinfo->getHostIP();
        uint16_t ssh_port = iptableinfo->getSSHPort();
        uint16_t rdp_port = iptableinfo->getRDPPort();
        std::vector<std::string> custom_port = iptableinfo->getCustomPort();
        std::string task_local_ip = iptableinfo->getTaskLocalIP();
        std::string public_ip = iptableinfo->getPublicIP();

        shell_add_iptable_to_system(task_id, host_ip, ssh_port, rdp_port, custom_port, task_local_ip, public_ip);
    }
}

void TaskManager::shell_remove_iptable_from_system(const std::string& task_id, const std::string &host_ip,
                                                   uint16_t ssh_port, const std::string &task_local_ip) {
    auto func_remove_chain = [](std::string main_chain, std::string task_chain) {
        std::string cmd = "sudo iptables -t nat -F " + task_chain;
        run_shell(cmd);
        cmd = "sudo iptables -t nat -D " + main_chain + " -j " + task_chain;
        run_shell(cmd);
        cmd = "sudo iptables -t nat -X " + task_chain;
        run_shell(cmd);
    };

    // remove chain
    std::string chain_name = "chain_" + task_id;
    func_remove_chain("PREROUTING", chain_name);
    func_remove_chain("PREROUTING", chain_name + "_dnat");
    func_remove_chain("POSTROUTING", chain_name + "_snat");

    // remove old rules
    std::string cmd = "sudo iptables --table nat -D PREROUTING --protocol tcp --destination " + host_ip
        + " --destination-port " + std::to_string(ssh_port) + " --jump DNAT --to-destination " + task_local_ip + ":22";
    run_shell(cmd.c_str());

    cmd = "sudo iptables -t nat -D PREROUTING -p tcp --dport " + std::to_string(ssh_port) 
        + " -j DNAT --to-destination " + task_local_ip + ":22";
    run_shell(cmd.c_str());

    auto pos = task_local_ip.rfind('.');
    std::string ip = task_local_ip.substr(0, pos) + ".1";
    cmd = "sudo iptables -t nat -D POSTROUTING -p tcp --dport " + std::to_string(ssh_port) 
        + " -d " + task_local_ip + " -j SNAT --to " + ip;
    run_shell(cmd.c_str());

    cmd = "sudo iptables -t nat -D PREROUTING -p tcp -m tcp --dport 20000:60000 -j DNAT --to-destination " 
        + task_local_ip + ":20000-60000";
    run_shell(cmd.c_str());

    cmd = "sudo iptables -t nat -D PREROUTING -p udp -m udp --dport 20000:60000 -j DNAT --to-destination " 
        + task_local_ip + ":20000-60000";
    run_shell(cmd.c_str());

    cmd = "sudo iptables -t nat -D POSTROUTING -d " + task_local_ip +
          " -p tcp -m tcp --dport 20000:60000 -j SNAT --to-source " + host_ip;
    run_shell(cmd.c_str());

    cmd = "sudo iptables -t nat -D POSTROUTING -d " + task_local_ip +
          " -p udp -m udp --dport 20000:60000 -j SNAT --to-source " + host_ip;
    run_shell(cmd.c_str());

    cmd = "sudo iptables -t nat -D PREROUTING -p tcp -d " + host_ip + " --dport " + std::to_string(ssh_port)
          + " -j DNAT --to-destination " + task_local_ip + ":22";
    run_shell(cmd.c_str());

    pos = task_local_ip.rfind('.');
    ip = task_local_ip.substr(0, pos) + ".0/24";
    cmd = "sudo iptables -t nat -D POSTROUTING -s " + ip + " -j SNAT --to-source " + host_ip;
    run_shell(cmd.c_str());

    cmd = "sudo iptables -t nat -D PREROUTING -p tcp -d " + host_ip + " --dport 6000:60000 -j DNAT --to-destination "
          + task_local_ip + ":6000-60000";
    run_shell(cmd.c_str());
}

void TaskManager::shell_add_iptable_to_system(const std::string& task_id, const std::string &host_ip,
                                              uint16_t ssh_port, uint16_t rdp_port,
                                              const std::vector<std::string>& custom_port,
                                              const std::string &task_local_ip, const std::string& public_ip) {
	auto func_remove_chain = [](std::string main_chain, std::string task_chain) {
		std::string cmd = "sudo iptables -t nat -F " + task_chain;
		run_shell(cmd);
		cmd = "sudo iptables -t nat -D " + main_chain + " -j " + task_chain;
		run_shell(cmd);
		cmd = "sudo iptables -t nat -X " + task_chain;
		run_shell(cmd);
	};

    // remove chain
    std::string chain_name = "chain_" + task_id;
    func_remove_chain("PREROUTING", chain_name);
    func_remove_chain("PREROUTING", chain_name + "_dnat");
    func_remove_chain("POSTROUTING", chain_name + "_snat");

    if (public_ip.empty()) {
        chain_name += "_dnat";

        // add chain and rules
        std::string cmd = "sudo iptables -t nat -N " + chain_name;
        run_shell(cmd);
        if (ssh_port > 0) {
            cmd = "sudo iptables -t nat -A " + chain_name + " -p tcp --destination " + host_ip + " --dport " 
                + std::to_string(ssh_port) + " --jump DNAT --to-destination " + task_local_ip + ":22";
            run_shell(cmd);
        }
        if (rdp_port > 0) {
            cmd = "sudo iptables -t nat -A " + chain_name + " -p tcp --destination " + host_ip + " --dport " 
                + std::to_string(rdp_port) + " --jump DNAT --to-destination " + task_local_ip + ":3389";
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
				cmd = "sudo iptables -t nat -A " + chain_name + " -p " + s_protocol + " --destination " + host_ip
					+ " --dport " + s_port + " --jump DNAT --to-destination " + task_local_ip + ":" + s_port;
				run_shell(cmd);
				continue;
			}

			if (s_port.find(':') != std::string::npos) {
				std::vector<std::string> vec = util::split(s_port, ":");
				if (vec.size() == 2 && util::is_digits(vec[0]) && util::is_digits(vec[1])) {
					cmd = "sudo iptables -t nat -A " + chain_name + " -p " + s_protocol + " --destination " 
                        + host_ip + " --dport " + vec[0] + " --jump DNAT --to-destination " + task_local_ip + ":" + vec[1];
					run_shell(cmd);
					continue;
				}
			}

            if (s_port.find('-') != std::string::npos) {
                std::vector<std::string> vec = util::split(s_port, "-");
                if (vec.size() == 2 && util::is_digits(vec[0]) && util::is_digits(vec[1])) {
                    cmd = "sudo iptables -t nat -A " + chain_name + " -p " + s_protocol + " --destination " + host_ip 
                        + " --dport " + vec[0] + ":" + vec[1] + " -j DNAT --to-destination " + task_local_ip + ":" +
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
                        cmd = "sudo iptables -t nat -A " + chain_name + " -p " + s_protocol + " --destination " + host_ip 
                            + " --dport " + vec1[0] + ":" + vec1[1] + " -j DNAT --to-destination " + task_local_ip + ":" 
                            + vec2[0] + "-" + vec2[1];
                        run_shell(cmd);
                        continue;
                    }
                }
            }
        }

        // add ref
        cmd = "sudo iptables -t nat -I PREROUTING -j " + chain_name;
        run_shell(cmd);
    } else {
        std::string chain_name_dnat = chain_name + "_dnat";
        std::string chain_name_snat = chain_name + "_snat";

        // add chain and rules
        std::string cmd = "sudo iptables -t nat -N " + chain_name_dnat;
        run_shell(cmd);
        cmd = "sudo iptables -t nat -A " + chain_name_dnat + " --destination " + public_ip +
            " --jump DNAT --to-destination " + task_local_ip;
        run_shell(cmd);

        cmd = "sudo iptables -t nat -N " + chain_name_snat;
        run_shell(cmd);
        cmd = "sudo iptables -t nat -I " + chain_name_snat + " --source " + task_local_ip +
            " --jump SNAT --to-source " + public_ip;
        run_shell(cmd);

        // add ref
        cmd = "sudo iptables -t nat -I PREROUTING -j " + chain_name_dnat;
        run_shell(cmd);
        cmd = "sudo iptables -t nat -I POSTROUTING -j " + chain_name_snat;
        run_shell(cmd);
    }
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
    int32_t hasNvram = VmClient::instance().IsDomainHasNvram(task_id);
    if (hasNvram > 0) undefineFlags |= VIR_DOMAIN_UNDEFINE_NVRAM;

    // undefine task
    VmClient::instance().DestroyAndUndefineDomain(task_id, undefineFlags);

    auto taskinfo = TaskInfoMgr::instance().getTaskInfo(task_id);
    if (taskinfo == nullptr) return;
    
    // leave vxlan network
    p2p_lan_service::instance().send_network_leave_request(taskinfo->getNetworkName(), task_id);

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

// create task
FResult TaskManager::createTask(const std::string& wallet, 
                                const std::shared_ptr<dbc::node_create_task_req_data>& data,
                                int64_t rent_end, USER_ROLE role, std::string& task_id) {
    CreateTaskParams create_params;
    FResult fret = parse_create_params(data->additional, role, create_params);
    if (fret.errcode != ERR_SUCCESS) {
        return fret;
    }

	std::vector<std::string> image_files;
	getNeededBackingImage(create_params.image_name, image_files);
	if (!image_files.empty()) {
		std::string str = "image backfile not exist: ";
		for (size_t i = 0; i < image_files.size(); i++) {
			if (i > 0)
				str += ",";
			str += image_files[i];
		}

        return FResult(ERR_ERROR, str);
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
    taskinfo->setTaskStatus(TaskStatus::TS_Task_Creating);
    TaskInfoMgr::instance().add(taskinfo);

    // add disks
	std::string disk_vda_file = "/data/vm_" + std::to_string(rand() % 100000) + "_" + util::time2str(tnow) + ".qcow2";
    std::shared_ptr<DiskInfo> disk_vda = std::make_shared<DiskInfo>();
    disk_vda->setName("vda");
    disk_vda->setSourceFile(disk_vda_file);
    disk_vda->setVirtualSize(g_disk_system_size * 1024L * 1024L * 1024L);
    TaskDiskMgr::instance().addDisk(task_id, disk_vda);

    std::string disk_vdb_file;
	if (create_params.data_file_name.empty()) {
		disk_vdb_file = "/data/data_" + std::to_string(rand() % 100000) + "_" + util::time2str(tnow) + ".qcow2";
	}
	else {
        disk_vdb_file = "/data/" + create_params.data_file_name;
	}
    std::shared_ptr<DiskInfo> disk_vdb = std::make_shared<DiskInfo>();
    disk_vdb->setName("vdb");
    disk_vdb->setSourceFile(disk_vdb_file);
    disk_vdb->setVirtualSize(create_params.disk_size * 1024L);
    TaskDiskMgr::instance().addDisk(task_id, disk_vdb);

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
    WalletRentTaskMgr::instance().add(wallet, task_id, rent_end);

    // send join network request
    p2p_lan_service::instance().send_network_join_request(create_params.network_name, task_id);
    
    // push event
    auto ev = std::make_shared<CreateTaskEvent>(task_id);
	this->pushTaskEvent(ev);

    return FResultOk;
}

FResult TaskManager::parse_create_params(const std::string &additional, USER_ROLE role, CreateTaskParams& params) {
    if (additional.empty()) {
        return FResult(ERR_ERROR, "additional is empty");
    }

    rapidjson::Document doc;
    doc.Parse(additional.c_str());
    if (!doc.IsObject()) {
        return FResult(ERR_ERROR, "additional parse failed");
    }

    std::string image_name, s_ssh_port, s_rdp_port,
        s_gpu_count, s_cpu_cores, s_mem_size, s_disk_size,
        s_vnc_port, data_file_name, operation_system, bios_mode,
        desc, network_name, public_ip;
    std::vector<std::string> custom_ports, multicast, nwfilter;

    JSON_PARSE_STRING(doc, "image_name", image_name)
    JSON_PARSE_STRING(doc, "desc", desc) 
    JSON_PARSE_STRING(doc, "ssh_port", s_ssh_port) 
    JSON_PARSE_STRING(doc, "rdp_port", s_rdp_port) 
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
    //"nwfilter": [
    //    "in,tcp,22,0.0.0.0/0,accept",
    //    "in,all,all,0.0.0.0/0,drop",
    //    "out,all,all,0.0.0.0/0,accept"
    //]
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
    JSON_PARSE_STRING(doc, "gpu_count", s_gpu_count)
    JSON_PARSE_STRING(doc, "cpu_cores", s_cpu_cores)
    JSON_PARSE_STRING(doc, "mem_size", s_mem_size)      //G
    JSON_PARSE_STRING(doc, "disk_size", s_disk_size)    //G
    JSON_PARSE_STRING(doc, "vnc_port", s_vnc_port)
    JSON_PARSE_STRING(doc, "data_file_name", data_file_name)
    JSON_PARSE_STRING(doc, "operation_system", operation_system)
    JSON_PARSE_STRING(doc, "bios_mode", bios_mode)
    JSON_PARSE_STRING(doc, "network_name", network_name)
    JSON_PARSE_STRING(doc, "public_ip", public_ip)
    //operation_system: "win"/"ubuntu"/""
    if (operation_system.empty()) {
        operation_system = "generic";
    }

    // rdp_port
    if (operation_system.find("win") != std::string::npos) {
        if (s_rdp_port.empty() || !util::is_digits(s_rdp_port) || atoi(s_rdp_port.c_str()) <= 0) {
            return FResult(ERR_ERROR, "rdp_port is invalid");
        }
        if (check_iptables_port_occupied(atoi(s_rdp_port))) {
            return FResult(ERR_ERROR, "rdp_port is occupied");
        }
    }
    // ssh_port
    else {
        if (s_ssh_port.empty() || !util::is_digits(s_ssh_port) || atoi(s_ssh_port.c_str()) <= 0) {
            return FResult(ERR_ERROR, "ssh_port is invalid");
        }
        if (check_iptables_port_occupied(atoi(s_ssh_port))) {
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
        FResult fret = VxlanManager::instance().CreateNetworkClient(network_name);
        if (fret.errcode == ERR_SUCCESS) {
            // do nothing
        } else {
            return fret;
        }
    }

    if (!public_ip.empty()) {
        ip_validator ip_vdr;
        variable_value val_ip(public_ip, false);
        if (!ip_vdr.validate(val_ip))
            return FResult(ERR_ERROR, "invalid public ip");
    }

    std::string login_password = genpwd();

    std::string task_id;
    int32_t cpu_cores = 0, sockets = 0, cores = 0, threads = 0;
    int32_t gpu_count = 0;
    std::map<std::string, std::list<std::string>> gpus;
    int64_t mem_size = 0; // KB
    int64_t disk_size = 0; // KB
    std::string vnc_password = login_password;

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
    if (role == USER_ROLE::Verifier) {
        task_id = "vm_check_" + std::to_string(time(nullptr));
    }

    // cpu
    if (!util::is_digits(s_cpu_cores) || atoi(s_cpu_cores.c_str()) <= 0) {
        return FResult(ERR_ERROR, "cpu_cores is invalid");
    }

    cpu_cores = atoi(s_cpu_cores.c_str());

    if (role == USER_ROLE::Verifier)
        cpu_cores = SystemInfo::instance().GetCpuInfo().cores;

    // gpu
    if (!util::is_digits(s_gpu_count) || atoi(s_gpu_count.c_str()) < 0) {
        return FResult(ERR_ERROR, "gpu_count is invalid");
    }

    gpu_count = atoi(s_gpu_count.c_str());

    if (role == USER_ROLE::Verifier)
        gpu_count = SystemInfo::instance().GetGpuInfo().size();

    // mem (G)
    if (!util::is_digits(s_mem_size) || atoi(s_mem_size.c_str()) <= 0) {
        return FResult(ERR_ERROR, "mem_size is invalid");
    }

    mem_size = atoi(s_mem_size.c_str()) * 1024L * 1024L;

    if (role == USER_ROLE::Verifier)
        mem_size = SystemInfo::instance().GetMemInfo().free;

    // disk (G)
    if (!util::is_digits(s_disk_size) || atoi(s_disk_size.c_str()) <= 0) {
        return FResult(ERR_ERROR, "disk_size is invalid");
    }

    disk_size = atoi(s_disk_size.c_str()) * 1024L * 1024L;
    if (role == USER_ROLE::Verifier)
        disk_size = (SystemInfo::instance().GetDiskInfo().available - g_disk_system_size * 1024L * 1024L) * 0.75;

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
 
	// params
	params.task_id = task_id;
	params.login_password = login_password;
	params.image_name = image_name;
	params.desc = desc;
	params.data_file_name = data_file_name;
    params.ssh_port = (uint16_t) atoi(s_ssh_port.c_str());
    params.rdp_port = (uint16_t) atoi(s_rdp_port.c_str());
    params.custom_port = custom_ports;
    params.cpu_sockets = sockets;
    params.cpu_cores = cores;
    params.cpu_threads = threads;
    params.gpus = gpus;
    params.mem_size = mem_size;
    params.disk_size = disk_size;
    params.vnc_port = vnc_port;
    params.vnc_password = vnc_password;
    params.operation_system = operation_system;
    params.bios_mode = bios_mode;
    params.multicast = multicast;
    params.network_name = network_name;
    params.public_ip = public_ip;
    params.nwfilter = nwfilter;

    return FResultOk;
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
    auto taskinfos = TaskInfoMgr::instance().getAllTaskInfos();
    for (auto& iter : taskinfos) {
        virDomainState st = VmClient::instance().GetDomainStatus(iter.first);
        if (st != VIR_DOMAIN_SHUTOFF) {
            auto gpus = TaskGpuMgr::instance().getTaskGpus(iter.first);
            if (!gpus.empty()) {
                for (auto &iter_gpu : gpus) {
                    can_use_gpu.erase(iter_gpu.first);
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
    return disk_size > 0 && disk_size <= (SystemInfo::instance().GetDiskInfo().available - g_disk_system_size * 1024L * 1024L);
}

bool TaskManager::check_iptables_port_occupied(uint16_t port) {
	bool bFind = false;
	auto taskinfos = TaskInfoMgr::instance().getAllTaskInfos();
	for (const auto& iter : taskinfos) {
		if (iter.second->getSSHPort() == port || iter.second->getRDPPort() == port) {
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

    if (ImageManager::instance().isDownloading(image_name) ||
        ImageManager::instance().isUploading(image_name)) {
        return FResult(ERR_ERROR, "image:" + image_name + " in downloading or uploading, please try again later");
    }

    return FResultOk;
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
    return FResultOk;
}

FResult TaskManager::check_cpu(int32_t sockets, int32_t cores, int32_t threads) {
    int32_t cpu_cores = sockets * cores * threads;
    if (cpu_cores <= 0 || cpu_cores > SystemInfo::instance().GetCpuInfo().cores ||
        sockets > SystemInfo::instance().GetCpuInfo().physical_cpus ||
        cores > SystemInfo::instance().GetCpuInfo().physical_cores_per_cpu ||
        threads > SystemInfo::instance().GetCpuInfo().threads_per_cpu) {
        return FResult(ERR_ERROR, "cpu config is invalid");
    }

    return FResultOk;
}

FResult TaskManager::check_gpu(const std::map<std::string, std::list<std::string>> &gpus) {
    std::map<std::string, gpu_info> can_use_gpu = SystemInfo::instance().GetGpuInfo();
    auto taskinfos = TaskInfoMgr::instance().getAllTaskInfos();
    for (auto& iter : taskinfos) {
        virDomainState st = VmClient::instance().GetDomainStatus(iter.first);
        if (st != VIR_DOMAIN_SHUTOFF) {
            auto gpus = TaskGpuMgr::instance().getTaskGpus(iter.first);
            if (!gpus.empty()) {
                for (auto &it1: gpus) {
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

        for (auto& it_device : it.second) {
            std::list<std::string>& can_use_devices = it_gpu->second.devices;
            auto found = std::find(can_use_devices.begin(), can_use_devices.end(), it_device);
            if (found == can_use_devices.end()) {
                fret = FResult(ERR_ERROR, "gpu device " + it_device + " not found in system");
                break;
            }
        }

        if (fret.errcode != ERR_SUCCESS) break;
    }

    return fret;
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
        return FResultOk;
}

FResult TaskManager::check_operation_system(const std::string& os) {
    if (os.empty()) return FResultOk;
    if (os == "generic") return FResultOk;
    if (os.find("ubuntu") != std::string::npos) return FResultOk;
    if (os.find("win") != std::string::npos) return FResultOk;
    return FResult(ERR_ERROR, "unsupported operation system");
}

FResult TaskManager::check_bios_mode(const std::string& bios_mode) {
    if (bios_mode.empty()) return FResultOk;
    if (bios_mode == "legacy") return FResultOk;
    if (bios_mode == "uefi") return FResultOk;
    return FResult(ERR_ERROR, "bios mode only supported [legacy] or [uefi]");
}

FResult TaskManager::check_multicast(const std::vector<std::string>& multicast) {
    if (multicast.empty()) return FResultOk;

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
    return FResultOk;
}

std::shared_ptr<TaskInfo> TaskManager::findTask(const std::string& wallet, const std::string &task_id) {
    std::shared_ptr<TaskInfo> taskinfo = nullptr;
    auto renttask = WalletRentTaskMgr::instance().getWalletRentTask(wallet);
    if (renttask != nullptr) {
        auto ids = renttask->getTaskIds();
        for (auto &id : ids) {
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
        return FResult(ERR_ERROR, "task not exist");
    }

    if (!VmClient::instance().IsExistDomain(task_id)) {
        return FResult(ERR_ERROR, "task not exist");
    }

    //TODO： check task resource


    virDomainState vm_status = VmClient::instance().GetDomainStatus(task_id);
    if (vm_status == VIR_DOMAIN_NOSTATE || vm_status == VIR_DOMAIN_SHUTOFF || vm_status == VIR_DOMAIN_PMSUSPENDED) {
        taskinfo->setTaskStatus(TaskStatus::TS_Task_Starting);
        
        auto ev = std::make_shared<StartTaskEvent>(task_id);
        this->pushTaskEvent(ev);

        return FResultOk;
    } else {
        return FResult(ERR_ERROR, "task is " + vm_status_string(vm_status));
    }
}

FResult TaskManager::shutdownTask(const std::string& wallet, const std::string &task_id) {
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

FResult TaskManager::poweroffTask(const std::string& wallet, const std::string& task_id) {
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
	}
	else {
		return FResult(ERR_ERROR, "task is " + vm_status_string(vm_status));
	}
}

FResult TaskManager::restartTask(const std::string& wallet, const std::string &task_id, bool force_reboot) {
    auto taskinfo = TaskInfoMgr::instance().getTaskInfo(task_id);
    if (taskinfo == nullptr) {
        return FResult(ERR_ERROR, "task not exist");
    }

    if (!VmClient::instance().IsExistDomain(task_id)) {
        return FResult(ERR_ERROR, "task not exist");
    }

    // TODO: check task resource


    virDomainState vm_status = VmClient::instance().GetDomainStatus(task_id);
    if (vm_status == VIR_DOMAIN_NOSTATE || vm_status == VIR_DOMAIN_SHUTOFF || vm_status == VIR_DOMAIN_RUNNING) {
        taskinfo->setTaskStatus(TaskStatus::TS_Task_Restarting);

        if (force_reboot) {
            auto ev = std::make_shared<ForceRebootTaskEvent>(task_id);
            this->pushTaskEvent(ev);
        }
        else {
            auto ev = std::make_shared<ReStartTaskEvent>(task_id);
            this->pushTaskEvent(ev);
        }

        return FResultOk;
    } else {
        return FResult(ERR_ERROR, "task is " + vm_status_string(vm_status));
    }
}

FResult TaskManager::resetTask(const std::string& wallet, const std::string &task_id) {
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

FResult TaskManager::deleteTask(const std::string& wallet, const std::string &task_id) {
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

FResult TaskManager::modifyTask(const std::string& wallet, 
    const std::shared_ptr<dbc::node_modify_task_req_data>& data) {
    std::string task_id = data->task_id;
    auto taskinfoPtr = TaskInfoManager::instance().getTaskInfo(task_id);
    if (taskinfoPtr == nullptr) {
        return FResult(ERR_ERROR, "task not exist");
    }

	if (!VmClient::instance().IsExistDomain(task_id)) {
		return FResult(ERR_ERROR, "task not exist");
	}

    virDomainState vm_status = VmClient::instance().GetDomainStatus(task_id);
    if (vm_status != VIR_DOMAIN_SHUTOFF) {
        return FResult(ERR_ERROR, "task is running, please close it first");
    }

    rapidjson::Document doc;
    doc.Parse(data->additional.c_str());
    if (!doc.IsObject()) {
        return FResult(ERR_ERROR, "additional parse failed");
    }

    uint16_t old_ssh_port, new_ssh_port;
    std::string s_new_ssh_port;
    JSON_PARSE_STRING(doc, "new_ssh_port", s_new_ssh_port);
    if (!s_new_ssh_port.empty()) {
        new_ssh_port = (uint16_t) atoi(s_new_ssh_port.c_str());
        if (new_ssh_port <= 0) {
            return FResult(ERR_ERROR, "new_ssh_port " + s_new_ssh_port + " is invalid! (usage: > 0)");
        }
        bool bFound = false;
        auto taskinfos = TaskInfoMgr::instance().getAllTaskInfos();
        for (const auto &iter : taskinfos) {
            if (iter.first != task_id) {
                if (iter.second->getSSHPort() == new_ssh_port || iter.second->getRDPPort() == new_ssh_port) {
                    bFound = true;
                    break;
                }
            }
        }
        if (bFound) {
            return FResult(ERR_ERROR, "new_ssh_port " + s_new_ssh_port + " has been used!");
        }

        old_ssh_port = taskinfoPtr->getSSHPort();
        taskinfoPtr->setSSHPort(new_ssh_port);
        TaskInfoMgr::instance().update(taskinfoPtr);

        auto taskIptablePtr = TaskIptableManager::instance().getIptableInfo(task_id);
        if (taskIptablePtr != nullptr) {
            taskIptablePtr->setSSHPort(new_ssh_port);
            TaskIptableManager::instance().update(taskIptablePtr);
        }
    }

    uint16_t new_rdp_port;
    std::string s_new_rdp_port;
    JSON_PARSE_STRING(doc, "new_rdp_port", s_new_rdp_port);
    if (!s_new_rdp_port.empty()) {
        new_rdp_port = (uint16_t) atoi(s_new_rdp_port.c_str());
        if (new_rdp_port <= 0) {
            return FResult(ERR_ERROR, "new_rdp_port " + s_new_rdp_port + " is invalid! (usage: > 0)");
        }
        bool bFound = false;
        auto task_list = TaskInfoMgr::instance().getAllTaskInfos();
        for (const auto &iter : task_list) {
            if (iter.first != task_id) {
                if (iter.second->getSSHPort() == new_rdp_port || iter.second->getRDPPort() == new_rdp_port) {
                    bFound = true;
                    break;
                }
            }
        }
        if (bFound) {
            return FResult(ERR_ERROR, "new_rdp_port " + s_new_rdp_port + " has been used!");
        }

        taskinfoPtr->setRDPPort(new_rdp_port);
        TaskInfoMgr::instance().update(taskinfoPtr);

        auto taskIptablePtr = TaskIptableManager::instance().getIptableInfo(task_id);
        if (taskIptablePtr != nullptr) {
            taskIptablePtr->setRDPPort(new_rdp_port);
            TaskIptableManager::instance().update(taskIptablePtr);
        }
    }

    if (!s_new_ssh_port.empty() && !s_new_rdp_port.empty()
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
                taskinfoPtr->setCustomPort(new_custom_port);
                TaskInfoMgr::instance().update(taskinfoPtr);

                auto taskIptablePtr = TaskIptableManager::instance().getIptableInfo(task_id);
                if (taskIptablePtr != nullptr) {
                    taskIptablePtr->setCustomPort(new_custom_port);
                    TaskIptableManager::instance().update(taskIptablePtr);
                }
            }
        }
    }

	auto taskIptablePtr = TaskIptableManager::instance().getIptableInfo(task_id);
    if (taskIptablePtr != nullptr) {
        shell_remove_iptable_from_system(task_id, taskIptablePtr->getHostIP(), old_ssh_port, 
            taskIptablePtr->getTaskLocalIP());
    }

    std::string new_public_ip;
    JSON_PARSE_STRING(doc, "new_public_ip", new_public_ip);
    if (!new_public_ip.empty()) {
        ip_validator ip_vdr;
        variable_value val_ip(new_public_ip, false);
        if (!ip_vdr.validate(val_ip))
            return FResult(ERR_ERROR, "invalid new public ip");

        taskinfoPtr->setPublicIP(new_public_ip);
        TaskInfoManager::instance().update(taskinfoPtr);

        auto taskIptablePtr = TaskIptableManager::instance().getIptableInfo(task_id);
        if (taskIptablePtr != nullptr) {
            taskIptablePtr->setPublicIP(new_public_ip);
            TaskIptableManager::instance().update(taskIptablePtr);
        }
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

            if (!new_nwfiter.empty()) {
                taskinfoPtr->setNwfilter(new_nwfiter);
                TaskInfoManager::instance().update(taskinfoPtr);

                ERRCODE ret = VmClient::instance().DefineNWFilter(task_id, new_nwfiter);
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
        if (new_vnc_port < 5900 || new_vnc_port > 6000) {
            return FResult(ERR_ERROR, "new_vnc_port " + s_new_vnc_port + " is invalid! (usage: 5900 =< port <= 6000)");
        }

        taskinfoPtr->setVncPort(new_vnc_port);
	}

    std::string new_gpu_count;
    std::map<std::string, std::list<std::string>> new_gpus;
    JSON_PARSE_STRING(doc, "new_gpu_count", new_gpu_count);
    if (!new_gpu_count.empty()) {
		int count = (uint16_t) atoi(new_gpu_count.c_str());
		if (allocate_gpu(count, new_gpus)) {
            std::map<std::string, std::shared_ptr<GpuInfo>> mp_gpus;
            for (auto& iter_gpu : new_gpus) {
                std::shared_ptr<GpuInfo> ptr = std::make_shared<GpuInfo>();
                ptr->setId(iter_gpu.first);
                for (auto& iter_device_id : iter_gpu.second) {
                    ptr->addDeviceId(iter_device_id);
                }
                mp_gpus.insert({ ptr->getId(), ptr });
            }

			TaskGpuMgr::instance().setTaskGpus(task_id, mp_gpus);
		}
		else {
			return FResult(ERR_ERROR, "allocate gpu failed");
		}
    }
    
    std::string s_new_cpu_cores;
    JSON_PARSE_STRING(doc, "new_cpu_cores", s_new_cpu_cores);
    if (!s_new_cpu_cores.empty()) {
        int32_t new_cpu_cores = atoi(s_new_cpu_cores);
		int32_t cpu_sockets = 1, cpu_cores = 1, cpu_threads = 2;
		if (allocate_cpu(new_cpu_cores, cpu_sockets, cpu_cores, cpu_threads)) {
            taskinfoPtr->setCpuSockets(cpu_sockets);
            taskinfoPtr->setCpuCoresPerSocket(cpu_cores);
            taskinfoPtr->setCpuThreadsPerCore(cpu_threads);
		}
		else {
			return FResult(ERR_ERROR, "allocate cpu failed");
		}
    }

    std::string new_mem_size;
    JSON_PARSE_STRING(doc, "new_mem_size", new_mem_size);
    if (!new_mem_size.empty()) {
        int64_t ksize = atoi(new_mem_size.c_str()) * 1024L * 1024L;
        if (allocate_mem(ksize)) {
            taskinfoPtr->setMemSize(ksize);
        }
        else {
            run_shell("echo 3 > /proc/sys/vm/drop_caches");

            if (allocate_mem(ksize)) {
                taskinfoPtr->setMemSize(ksize);
            } else {
                return FResult(ERR_ERROR, "allocate memory failed");
            }
        } 
    }

    // being to redefine domain
	FResult fret = VmClient::instance().RedefineDomain(taskinfoPtr);
	if (fret.errcode != ERR_SUCCESS) {
		return fret;
	}

    return FResultOk;
}

FResult TaskManager::getTaskLog(const std::string &task_id, QUERY_LOG_DIRECTION direction, int32_t nlines,
                                std::string &log_content) {
    auto taskinfo = TaskInfoMgr::instance().getTaskInfo(task_id);
    if (taskinfo == nullptr) {
        log_content = "";
        return FResult(ERR_ERROR, "task not exist");
    }

	if (!VmClient::instance().IsExistDomain(task_id)) {
		return FResult(ERR_ERROR, "task not exist");
	}
    
    return VmClient::instance().GetDomainLog(task_id, direction, nlines, log_content);
}

FResult TaskManager::listImages(const std::shared_ptr<dbc::node_list_images_req_data>& data,
    const AuthoriseResult& result, std::vector<ImageFile>& images) {
    try {
        ImageServer imgsvr;
        imgsvr.from_string(data->image_server);

        if (!result.success) {
            ImageMgr::instance().listLocalShareImages(imgsvr, images);
        }
        else {
            ImageMgr::instance().listWalletLocalShareImages(result.rent_wallet, imgsvr, images);
        }

        return FResultOk;
    }
    catch (std::exception& e) {
        return FResultOk;
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

    std::vector<ImageFile> images;
    ImageServer imgsvr;
    imgsvr.from_string(data->image_server);
    ImageManager::instance().listShareImages(imgsvr, images);
    auto iter = std::find_if(images.begin(), images.end(), [image_filename](const ImageFile& iter) {
        return iter.name == image_filename;
        });
    if (iter == images.end()) {
        return FResult(ERR_ERROR, "image:" + image_filename + " not exist");
    }

    if (ImageManager::instance().isDownloading(image_filename)) {
        return FResult(ERR_ERROR, "image:" + image_filename + " in downloading");
    }

    FResult fret = ImageManager::instance().download(image_filename, local_dir, imgsvr);
    if (fret.errcode != ERR_SUCCESS)
        return fret;
    else
        return FResultOk;
}

FResult TaskManager::downloadImageProgress(const std::string& wallet,
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
        rsp_json += ",\"progress\": \"" + std::to_string(int(progress * 100)) + "%\"";
    }
    else {
        rsp_json += "\"status\": \"done\"";
        rsp_json += ",\"progress\": \"100%\"";
    }
    rsp_json += "}";
    rsp_json += "}";

    return FResult(ERR_SUCCESS, rsp_json);
}

FResult TaskManager::stopDownloadImage(const std::string& wallet,
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

FResult TaskManager::uploadImageProgress(const std::string& wallet, const std::shared_ptr<dbc::node_upload_image_progress_req_data>& data) {
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
        rsp_json += ",\"progress\": \"" + std::to_string(int(progress * 100)) + "%\"";
    }
    else {
        rsp_json += "\"status\": \"done\"";
        rsp_json += ",\"progress\": \"100%\"";
    }
    rsp_json += "}";
    rsp_json += "}";

    return FResult(ERR_SUCCESS, rsp_json);
}

FResult TaskManager::stopUploadImage(const std::string& wallet, const std::shared_ptr<dbc::node_stop_upload_image_req_data>& data) {
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

FResult TaskManager::deleteImage(const std::string& wallet, const std::shared_ptr<dbc::node_delete_image_req_data>& data) {
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
        return FResult(ERR_ERROR, "image file:" + image_filename + " is not exist");
    }

    if (ImageManager::instance().isDownloading(image_filename) ||
        ImageManager::instance().isUploading(image_fullpath)) {
        return FResult(ERR_ERROR, "image file is in downloading or uploading, please try again later");
    }

    ImageManager::instance().deleteImage(image_fullpath);

    return FResultOk;
}

void TaskManager::listAllTask(const std::string& wallet, std::vector<std::shared_ptr<TaskInfo>> &vec) {
    auto renttask = WalletRentTaskMgr::instance().getWalletRentTask(wallet);
    if (renttask != nullptr) {
        std::vector<std::string> ids = renttask->getTaskIds();
        for (auto &it_id: ids) {
            auto taskinfo = TaskInfoMgr::instance().getTaskInfo(it_id);
            if (taskinfo != nullptr) {
                vec.push_back(taskinfo);
            }
        }
    }
}

void TaskManager::listRunningTask(const std::string& wallet, std::vector<std::shared_ptr<TaskInfo>> &vec) {
    auto renttask = WalletRentTaskMgr::instance().getWalletRentTask(wallet);
    if (renttask != nullptr) {
        std::vector<std::string> ids = renttask->getTaskIds();
        for (auto &it_id: ids) {
            auto taskinfo = TaskInfoMgr::instance().getTaskInfo(it_id);
            if (taskinfo != nullptr && taskinfo->getTaskStatus() == TaskStatus::TS_Task_Running) {
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
        if (taskinfo != nullptr && taskinfo->getTaskStatus() == TaskStatus::TS_Task_Running) {
            count += 1;
        }
    }

    return count;
}

TaskStatus TaskManager::queryTaskStatus(const std::string &task_id) {
    auto taskinfo = TaskInfoMgr::instance().getTaskInfo(task_id);
    if (taskinfo == nullptr) {
        return TaskStatus::TS_Task_Shutoff;
    } else {
        TaskStatus task_status = taskinfo->getTaskStatus();
        virDomainState domain_status = VmClient::instance().GetDomainStatus(task_id);
        
        if ((task_status == TaskStatus::TS_Task_Shutoff && domain_status != VIR_DOMAIN_SHUTOFF)
            || (task_status == TaskStatus::TS_Task_Running && domain_status != VIR_DOMAIN_RUNNING)
            || (task_status == TaskStatus::TS_Task_PMSuspend && domain_status != VIR_DOMAIN_PMSUSPENDED)) {
            if (domain_status == VIR_DOMAIN_SHUTOFF || domain_status == VIR_DOMAIN_SHUTDOWN) {
                task_status = TaskStatus::TS_Task_Shutoff;
			}
			else if (domain_status == VIR_DOMAIN_RUNNING || domain_status == VIR_DOMAIN_PAUSED
                     || domain_status == VIR_DOMAIN_BLOCKED) {
                task_status = TaskStatus::TS_Task_Running;
            }
            else if (domain_status == VIR_DOMAIN_PMSUSPENDED) {
                task_status = TaskStatus::TS_Task_PMSuspend;
            }

            taskinfo->setTaskStatus(task_status);
        }
 
        return task_status;
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

std::string TaskManager::createSessionId(const std::string &wallet, const std::vector<std::string>& multisig_signers) {
    auto wallet_session = WalletSessionIDMgr::instance().geWalletSessionIdByWallet(wallet);
    if (wallet_session == nullptr) {
        std::string session_id = util::create_session_id();
        std::shared_ptr<WalletSessionId> ptr = std::make_shared<WalletSessionId>();
        ptr->setRentWallet(wallet);
        ptr->setSessionId(session_id);
        ptr->setMultisigSigners(multisig_signers);
        WalletSessionIDMgr::instance().add(ptr);
        return session_id;
    } else {
        return wallet_session->getSessionId();
    }
}

std::string TaskManager::getSessionId(const std::string &wallet) {
    auto it = WalletSessionIDMgr::instance().geWalletSessionIdByWallet(wallet);
    if (it != nullptr) {
        return it->getSessionId();
    } else {
        return "";
    }
}

std::string TaskManager::checkSessionId(const std::string &session_id, const std::string &session_id_sign) {
    if (session_id.empty() || session_id_sign.empty())
        return "";

    auto it = WalletSessionIDMgr::instance().getWalletSessionIdBySessionId(session_id);
    if (it == nullptr) {
        return "";
    }

    if (util::verify_sign(session_id_sign, session_id, it->getRentWallet())) {
        return it->getRentWallet();
    }

    auto multisig_signers = it->getMultisigSigners();
    for (auto& it_wallet : multisig_signers) {
        if (util::verify_sign(session_id_sign, session_id, it_wallet)) {
            return it->getRentWallet();
        }
    }

    return "";
}

FResult TaskManager::listTaskSnapshot(const std::string& wallet, const std::string& task_id, std::vector<dbc::snapshot_info>& snapshots) {
    TaskDiskMgr::instance().listSnapshots(task_id, snapshots);
	return FResultOk;
}

FResult TaskManager::createSnapshot(const std::string& wallet, const std::string& task_id, const std::string& snapshot_name, const ImageServer& imgsvr, const std::string& desc) {
    FResult fret = TaskDiskMgr::instance().createAndUploadSnapshot(task_id, snapshot_name, imgsvr, desc);
    return fret;
}

FResult TaskManager::terminateSnapshot(const std::string& wallet, const std::string &task_id, const std::string& snapshot_name) {
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
            m_process_cond.wait(lock, [this] {
                return !m_running || !m_events.empty();
            });

            if (!m_running)
                break;

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
            TASK_LOG_ERROR(ev->task_id, "unknown task event_type:" + std::to_string((int) ev->type));
            break;
    }
    
    TaskInfoMgr::instance().update_running_tasks();
}

void TaskManager::process_create_task(const std::shared_ptr<TaskEvent>& ev) {
    std::shared_ptr<CreateTaskEvent> ev_createtask = std::dynamic_pointer_cast<CreateTaskEvent>(ev);
    if (ev_createtask == nullptr) return;

    auto taskinfo = TaskInfoMgr::instance().getTaskInfo(ev->task_id);
    if (taskinfo == nullptr) return;

	ERRCODE ret = VmClient::instance().DefineNWFilter(ev->task_id, taskinfo->getNwfilter());
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
    } else {
        std::string local_ip = VmClient::instance().GetDomainLocalIP(ev->task_id);
        if (!local_ip.empty()) {
            if (!VmClient::instance().SetDomainUserPassword(ev->task_id,
                    taskinfo->getOperationSystem().find("win") == std::string::npos ? g_vm_ubuntu_login_username : g_vm_windows_login_username,
                    taskinfo->getLoginPassword())) {
                VmClient::instance().DestroyDomain(ev->task_id);

                if (taskinfo->getTaskStatus() == TaskStatus::TS_Task_Creating) {
                    taskinfo->setTaskStatus(TaskStatus::TS_CreateTaskError);
                }
                TASK_LOG_ERROR(ev->task_id, "set domain password failed");
                return;
            }

			if (!create_task_iptable(ev->task_id, taskinfo->getSSHPort(), taskinfo->getRDPPort(),
			        taskinfo->getCustomPort(), local_ip, taskinfo->getPublicIP())) {
				VmClient::instance().DestroyDomain(ev->task_id);

				if (taskinfo->getTaskStatus() == TaskStatus::TS_Task_Creating) {
					taskinfo->setTaskStatus(TaskStatus::TS_CreateTaskError);
				}
                TASK_LOG_ERROR(ev->task_id, "create task iptable failed");
                return;
            }

			if (taskinfo->getTaskStatus() == TaskStatus::TS_Task_Creating) {
				taskinfo->setTaskStatus(TaskStatus::TS_Task_Running);
			}
            TASK_LOG_INFO(ev->task_id, "create task successful");
        } else {
            VmClient::instance().DestroyDomain(ev->task_id);

			if (taskinfo->getTaskStatus() == TaskStatus::TS_Task_Creating) {
				taskinfo->setTaskStatus(TaskStatus::TS_CreateTaskError);
			}

            TASK_LOG_ERROR(ev->task_id, "get domain local ip failed");
        }
    }
}

bool TaskManager::create_task_iptable(const std::string& task_id, uint16_t ssh_port,
	    uint16_t rdp_port, const std::vector<std::string>& custom_port,
	    const std::string& task_local_ip, const std::string& public_ip) {
	std::string host_ip = SystemInfo::instance().GetDefaultRouteIp();
	if (!host_ip.empty() && !task_local_ip.empty()) {
		shell_add_iptable_to_system(task_id, host_ip, ssh_port, rdp_port, custom_port, task_local_ip, public_ip);

		std::shared_ptr<IptableInfo> iptable = std::make_shared<IptableInfo>();
		iptable->setTaskId(task_id);
		iptable->setHostIP(host_ip);
		iptable->setTaskLocalIP(task_local_ip);
		iptable->setSSHPort(ssh_port);
		iptable->setRDPPort(rdp_port);
		iptable->setCustomPort(custom_port);
		iptable->setPublicIP(public_ip);
		TaskIptableMgr::instance().add(iptable);

		TASK_LOG_INFO(task_id, "create iptable successful,"
	        " host_ip:" + host_ip + " ssh_port:" + std::to_string(ssh_port) 
            + " rdp_port:" + std::to_string(rdp_port) 
            + " custom_port.size:" + std::to_string(custom_port.size())
			+ " task_local_ip:" + task_local_ip
            + " public_ip:" + public_ip);
		return true;
	}
	else {
		TASK_LOG_ERROR(task_id, "create iptable failed, host_ip or task_local_ip is empty: "
            << "host_ip:" << host_ip << ", task_local_ip:" << task_local_ip);
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

        std::string cmd_get_backing_file = "qemu-img info " + cur_image + " | grep -i 'backing file:' | awk -F ': ' '{print $2}'";
        cur_image = run_shell(cmd_get_backing_file);
    }
}

void TaskManager::process_start_task(const std::shared_ptr<TaskEvent>& ev) {
    std::shared_ptr<StartTaskEvent> ev_starttask = std::dynamic_pointer_cast<StartTaskEvent>(ev);
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
        if (taskinfo->getTaskStatus() == TaskStatus::TS_Task_Starting) {
            taskinfo->setTaskStatus(TaskStatus::TS_Task_Running);
        }
        add_iptable_to_system(ev->task_id);
        TASK_LOG_INFO(ev->task_id, "start task successful");
    }
}

void TaskManager::process_shutdown_task(const std::shared_ptr<TaskEvent>& ev) {
	std::shared_ptr<ShutdownTaskEvent> ev_shutdowntask = std::dynamic_pointer_cast<ShutdownTaskEvent>(ev);
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
    }
}

void TaskManager::process_poweroff_task(const std::shared_ptr<TaskEvent>& ev) {
	std::shared_ptr<PowerOffTaskEvent> ev_powerofftask = std::dynamic_pointer_cast<PowerOffTaskEvent>(ev);
	if (ev_powerofftask == nullptr) return;

	auto taskinfo = TaskInfoMgr::instance().getTaskInfo(ev->task_id);
	if (taskinfo == nullptr) return;

	ERRCODE ret = VmClient::instance().DestroyDomain(ev->task_id);
	if (ret != ERR_SUCCESS) {
        if (taskinfo->getTaskStatus() == TaskStatus::TS_Task_BeingPoweroff) {
            taskinfo->setTaskStatus(TaskStatus::TS_PoweroffTaskError);
        }
		TASK_LOG_ERROR(ev->task_id, "poweroff task failed");
	}
	else {
        if (taskinfo->getTaskStatus() == TaskStatus::TS_Task_BeingPoweroff) {
            taskinfo->setTaskStatus(TaskStatus::TS_Task_Shutoff);
        }
		remove_iptable_from_system(ev->task_id);
		TASK_LOG_INFO(ev->task_id, "poweroff task successful");
	}
}

void TaskManager::process_restart_task(const std::shared_ptr<TaskEvent>& ev) {
	std::shared_ptr<ReStartTaskEvent> ev_restarttask = std::dynamic_pointer_cast<ReStartTaskEvent>(ev);
	if (ev_restarttask == nullptr) return;

    auto taskinfo = TaskInfoMgr::instance().getTaskInfo(ev->task_id);
    if (taskinfo == nullptr) return;

    virDomainState vm_status = VmClient::instance().GetDomainStatus(ev->task_id);
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
            add_iptable_to_system(ev->task_id);
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
            add_iptable_to_system(ev->task_id);
            TASK_LOG_INFO(ev->task_id, "restart task successful");
        }
    }
}

void TaskManager::process_force_reboot_task(const std::shared_ptr<TaskEvent>& ev) {
	std::shared_ptr<ForceRebootTaskEvent> ev_forcereboottask = std::dynamic_pointer_cast<ForceRebootTaskEvent>(ev);
	if (ev_forcereboottask == nullptr) return;

    auto taskinfo = TaskInfoMgr::instance().getTaskInfo(ev->task_id);
    if (taskinfo == nullptr) return;

    virDomainState vm_status = VmClient::instance().GetDomainStatus(ev->task_id);
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
            add_iptable_to_system(ev->task_id);
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
            if ((ret = VmClient::instance().StartDomain(ev->task_id)) == ERR_SUCCESS) {
				if (taskinfo->getTaskStatus() == TaskStatus::TS_Task_Restarting) {
					taskinfo->setTaskStatus(TaskStatus::TS_Task_Running);
				}
                add_iptable_to_system(ev->task_id);
                TASK_LOG_INFO(ev->task_id, "restart task successful");
            } else {
				if (taskinfo->getTaskStatus() == TaskStatus::TS_Task_Restarting) {
					taskinfo->setTaskStatus(TaskStatus::TS_RestartTaskError);
				}
                TASK_LOG_ERROR(ev->task_id, "restart task failed");
            }
        }
    }
}

void TaskManager::process_reset_task(const std::shared_ptr<TaskEvent>& ev) {
	std::shared_ptr<ResetTaskEvent> ev_resettask = std::dynamic_pointer_cast<ResetTaskEvent>(ev);
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

void TaskManager::process_create_snapshot(const std::shared_ptr<TaskEvent>& ev) {
    TaskDiskMgr::instance().process_create_snapshot(ev);
}

void TaskManager::prune_task_thread_func() {
    while (m_running) {
		std::unique_lock<std::mutex> lock(m_prune_mtx);
		m_prune_cond.wait_for(lock, std::chrono::seconds(900), [this] {
			return !m_running || !m_events.empty();
		});

		if (!m_running)
			break;

        shell_remove_reject_iptable_from_system();

        MACHINE_STATUS machine_status = HttpDBCChainClient::instance().request_machine_status(
            ConfManager::instance().GetNodeId());
        if (machine_status == MACHINE_STATUS::Unknown) continue;

        std::string cur_renter;
        int64_t cur_rent_end = 0;
        HttpDBCChainClient::instance().request_cur_renter(ConfManager::instance().GetNodeId(), cur_renter, cur_rent_end);
        int64_t cur_block = HttpDBCChainClient::instance().request_cur_block();

        auto wallet_renttasks = WalletRentTaskMgr::instance().getAllWalletRentTasks();
        for (auto &it: wallet_renttasks) {
            if (machine_status == MACHINE_STATUS::Verify) {
                std::vector<std::string> ids = it.second->getTaskIds();
                for (auto &task_id: ids) {
                    if (task_id.find("vm_check_") == std::string::npos) {
                        TASK_LOG_INFO(task_id, "stop task and machine status is " << (int32_t) machine_status);
                        close_task(task_id);
                    }
                }
            } 
            else if (machine_status == MACHINE_STATUS::Online) {
                std::vector<std::string> ids = it.second->getTaskIds();
                for (auto &task_id: ids) {
                    if (task_id.find("vm_check_") == std::string::npos) {
                        TASK_LOG_INFO(task_id, "stop task and machine status is " << (int32_t) machine_status);
                        close_task(task_id);
                    } else {
                        TASK_LOG_INFO(task_id, "delete check task and machine status is " << (int32_t) machine_status);
                        delete_task(task_id);
                    }
                }
            } 
            else if (machine_status == MACHINE_STATUS::Rented) {
                std::vector<std::string> ids = it.second->getTaskIds();
                for (auto &task_id: ids) {
                    if (task_id.find("vm_check_") != std::string::npos) {
                        TASK_LOG_INFO(task_id, "delete check task and machine status is " << (int32_t) machine_status);
                        delete_task(task_id);
                    }
                }
            }

            if (machine_status == MACHINE_STATUS::Online || machine_status == MACHINE_STATUS::Rented) {
                if (it.first != cur_renter) {
                    std::vector<std::string> ids = it.second->getTaskIds();
                    for (auto &task_id: ids) {
                        TASK_LOG_INFO(task_id, "stop task and machine status: " << (int32_t) machine_status 
                            << ", cur_renter:" << cur_renter << ", cur_rent_end:" << cur_rent_end
                            << ", cur_wallet:" << it.first);
                        close_task(task_id);
                    }

                    // 1小时出120个块
                    int64_t wallet_rent_end = it.second->getRentEnd();
                    int64_t reserve_end = wallet_rent_end + 120 * 24 * 10; //保留10天
                    if (cur_block > 0 && reserve_end < cur_block) {
                        ids = it.second->getTaskIds();
                        for (auto &task_id: ids) {
                            TASK_LOG_INFO(task_id, "delete task and machine status: " << (int32_t) machine_status 
                                << " ,task rent end: " << it.second->getRentEnd() << " ,current block:" << cur_block);
                            delete_task(task_id);
                        }
                    }
                } else if (it.first == cur_renter) {
                    if (cur_rent_end > it.second->getRentEnd()) {
                        int64_t old_rent_end = it.second->getRentEnd();
                        it.second->setRentEnd(cur_rent_end);
                        WalletRentTaskMgr::instance().update(it.second);
                        LOG_INFO << "update rent_end, wallet=" << it.first
                            << ", update rent_end=old:" << old_rent_end << ",new:" << cur_rent_end;
                    }
                }
            }
        }

        if (cur_rent_end <= 0) {
            broadcast_message("empty");
        }
        else {
            broadcast_message("renting");
        }
    }
}

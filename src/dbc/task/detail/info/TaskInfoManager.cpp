#include "TaskInfoManager.h"
#include "log/log.h"
#include "task/vm/vm_client.h"
#include "tinyxml2.h"

FResult TaskInfoManager::init() {
    bool ret = m_db.init_db(EnvManager::instance().get_db_path(), "task.db");
    if (!ret) {
        return FResult(ERR_ERROR, "init task_db failed");
    }

    std::map<std::string, std::shared_ptr<dbc::db_task_info>> mp_tasks;
    m_db.load_datas(mp_tasks);

    for (auto& iter : mp_tasks) {
        if (!VmClient::instance().IsExistDomain(iter.first)) {
            continue;
        }

        std::shared_ptr<TaskInfo> taskinfo = std::make_shared<TaskInfo>();
        taskinfo->m_db_info = iter.second;

        std::string task_id = iter.first;
        std::string strXml = VmClient::instance().GetDomainXML(task_id);

        tinyxml2::XMLDocument doc;
        tinyxml2::XMLError err = doc.Parse(strXml.c_str());
        if (err != tinyxml2::XML_SUCCESS) {
            LOG_ERROR << " parse xml desc failed";
            continue;
        }
        tinyxml2::XMLElement* root = doc.RootElement();
        // cpu
        tinyxml2::XMLElement* ele_cpu = root->FirstChildElement("cpu");
        if (ele_cpu != nullptr) {
            tinyxml2::XMLElement* ele_topology = ele_cpu->FirstChildElement("topology");
            if (ele_topology != nullptr) {
                taskinfo->setCpuSockets(ele_topology->IntAttribute("sockets"));
                taskinfo->setCpuCoresPerSocket(ele_topology->IntAttribute("cores"));
                taskinfo->setCpuThreadsPerCore(ele_topology->IntAttribute("threads"));
            }
        }
        // mem
        tinyxml2::XMLElement* ele_memory = root->FirstChildElement("memory");
        if (ele_memory != nullptr) {
            const char* str_memory = ele_memory->GetText();
            taskinfo->setMemSize(atol(str_memory));
        }
        // vnc
        tinyxml2::XMLElement* ele_devices = root->FirstChildElement("devices");
        if (ele_devices != nullptr) {
            tinyxml2::XMLElement* ele_graphics = ele_devices->FirstChildElement("graphics");
            while (ele_graphics != nullptr) {
                std::string graphics_type = ele_graphics->Attribute("type");
                if (graphics_type == "vnc") {
                    std::string vnc_port = ele_graphics->Attribute("port");
                    std::string vnc_pwd = ele_graphics->Attribute("passwd");
                    taskinfo->setVncPort(vnc_port.empty() ? -1 : atoi(vnc_port.c_str()));
                    taskinfo->setVncPassword(vnc_pwd);
                    break;
                }
                ele_graphics = ele_graphics->NextSiblingElement("graphics");
            }
        }

        m_task_infos.insert({ task_id, taskinfo });
    }

	// init running tasks
	ret = m_running_db.init_db(EnvManager::instance().get_db_path(), "runningtasks.db");
	if (!ret) {
		return FResult(ERR_ERROR, "init running tasks db failed");
	}

	m_running_db.load_datas(m_running_tasks);

    for (std::vector<std::string>::iterator iter = m_running_tasks.begin(); iter != m_running_tasks.end(); ) {
        if (!VmClient::instance().IsExistDomain(*iter)) {
            iter = m_running_tasks.erase(iter);
        }
        else {
            iter++;
        }
    }

    return FResultOk;
}


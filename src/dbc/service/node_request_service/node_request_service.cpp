#include <boost/property_tree/json_parser.hpp>
#include <cassert>
#include <boost/exception/all.hpp>
#include "server.h"
#include "conf_manager.h"
#include "node_request_service.h"
#include "service_message_id.h"
#include "matrix_types.h"
#include "matrix_coder.h"
#include "ip_validator.h"
#include "port_validator.h"
#include "task_common_def.h"
#include "util.h"
#include <boost/thread/thread.hpp>
#include "service_topic.h"
#include <ctime>
#include <boost/format.hpp>
#include "url_validator.h"
#include "time_util.h"
#include <stdlib.h>
#include <boost/algorithm/string/join.hpp>
#include "message_id.h"

using namespace matrix::core;
using namespace matrix::service_core;
using namespace ai::dbc;

namespace dbc {
	std::string get_gpu_spec(const std::string& s) {
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

		}

		return rt;
	}

	std::string get_is_update(const std::string& s) {
		if (s.empty()) return "";

		std::string operation;
		std::stringstream ss;
		ss << s;
		boost::property_tree::ptree pt;

		try {
			boost::property_tree::read_json(ss, pt);
			if (pt.count("operation") != 0) {
				operation = pt.get<std::string>("operation");
			}
		}
		catch (...) {

		}

		return operation;
	}

	node_request_service::node_request_service() {

	}

	void node_request_service::init_timer() {
		// 配置文件：timer_ai_training_task_schedule_in_second（15秒）
		m_timer_invokers[AI_TRAINING_TASK_TIMER] = std::bind(&node_request_service::on_training_task_timer,
			this, std::placeholders::_1);
		m_training_task_timer_id = this->add_timer(AI_TRAINING_TASK_TIMER,
			CONF_MANAGER->get_timer_ai_training_task_schedule_in_second() *
			1000, ULLONG_MAX, DEFAULT_STRING);

		// 10分钟
		m_timer_invokers[AI_PRUNE_TASK_TIMER] = std::bind(&node_request_service::on_prune_task_timer, this,
			std::placeholders::_1);
		m_prune_task_timer_id = this->add_timer(AI_PRUNE_TASK_TIMER, AI_PRUNE_TASK_TIMER_INTERVAL, ULLONG_MAX,
			DEFAULT_STRING);

		assert(INVALID_TIMER_ID != m_training_task_timer_id);
		assert(INVALID_TIMER_ID != m_prune_task_timer_id);
	}

	void node_request_service::init_invoker() {
		invoker_type invoker;
		BIND_MESSAGE_INVOKER(NODE_CREATE_TASK_REQ, &node_request_service::on_node_create_task_req);
		BIND_MESSAGE_INVOKER(NODE_START_TASK_REQ, &node_request_service::on_node_start_task_req);
		BIND_MESSAGE_INVOKER(NODE_RESTART_TASK_REQ, &node_request_service::on_node_restart_task_req);
		BIND_MESSAGE_INVOKER(NODE_STOP_TASK_REQ, &node_request_service::on_node_stop_task_req);
		BIND_MESSAGE_INVOKER(NODE_TASK_LOGS_REQ, &node_request_service::on_node_task_logs_req);
		BIND_MESSAGE_INVOKER(NODE_LIST_TASK_REQ, &node_request_service::on_node_list_task_req);

		BIND_MESSAGE_INVOKER(typeid(get_task_queue_size_req_msg).name(),
			&node_request_service::on_get_task_queue_size_req);
	}

	void node_request_service::init_subscription() {
		SUBSCRIBE_BUS_MESSAGE(NODE_CREATE_TASK_REQ);
		SUBSCRIBE_BUS_MESSAGE(NODE_START_TASK_REQ);
		SUBSCRIBE_BUS_MESSAGE(NODE_RESTART_TASK_REQ);
		SUBSCRIBE_BUS_MESSAGE(NODE_STOP_TASK_REQ);
		SUBSCRIBE_BUS_MESSAGE(NODE_TASK_LOGS_REQ);
		SUBSCRIBE_BUS_MESSAGE(NODE_LIST_TASK_REQ);

		//task queue size query
		SUBSCRIBE_BUS_MESSAGE(typeid(get_task_queue_size_req_msg).name());
	}

	int32_t node_request_service::service_init(bpo::variables_map& options) {
		if (E_SUCCESS != m_task_scheduler.Init()) {
			return E_DEFAULT;
		}
		else {
			return E_SUCCESS;
		}
	}

	int32_t node_request_service::service_exit() {
		remove_timer(m_training_task_timer_id);
		remove_timer(m_prune_task_timer_id);
		return E_SUCCESS;
	}

	bool node_request_service::hit_node(const std::vector<std::string>& peer_node_list, const std::string& node_id) {
		bool hit = false;
		auto it = peer_node_list.begin();
		for (; it != peer_node_list.end(); it++) {
			if ((*it) == node_id) {
				hit = true;
				break;
			}
		}

		return hit;
	}

	bool node_request_service::check_nonce(const std::string& nonce) {
		if (!dbc::check_id(nonce)) {
			return false;
		}

		if (m_nonceCache.contains(nonce)) {
			return false;
		}
		else {
			m_nonceCache.insert(nonce, 1);
		}

		return true;
	}

	int32_t node_request_service::on_node_create_task_req(std::shared_ptr<dbc::network::message>& msg) {
		std::shared_ptr<matrix::service_core::node_create_task_req> req =
			std::dynamic_pointer_cast<matrix::service_core::node_create_task_req>(msg->get_content());
		if (req == nullptr) return E_DEFAULT;

		//防止同一个msg不停的重复被广播进入同一个节点，导致广播风暴
		if (!check_nonce(req->header.nonce)) {
			LOG_ERROR << "ai power provider service nonce error ";
			return E_DEFAULT;
		}

		if (!dbc::check_id(req->body.task_id)) {
			LOG_ERROR << "ai power provider service task_id error ";
			return E_DEFAULT;
		}

		//验证签名
		std::string sign_msg = req->body.task_id + req->header.nonce + req->body.additional;
		if (req->header.exten_info.size() < 3) {
			LOG_ERROR << "exten info error.";
			return E_DEFAULT;
		}
		if (!dbc::verify_sign(req->header.exten_info["sign"], sign_msg, req->header.exten_info["origin_id"])) {
			LOG_ERROR << "sign error." << req->header.exten_info["origin_id"];
			return E_DEFAULT;
		}

		// 检查是否命中当前节点
		const std::vector<std::string>& peer_nodes = req->body.peer_nodes_list;
		bool hit_self = hit_node(peer_nodes, CONF_MANAGER->get_node_id());
		if (hit_self) {
			return task_create(req);
		}
		else {
			CONNECTION_MANAGER->broadcast_message(msg, msg->header.src_sid);
			return E_SUCCESS;
		}
	}

	int32_t node_request_service::on_node_start_task_req(std::shared_ptr<dbc::network::message>& msg) {
		std::shared_ptr<matrix::service_core::node_start_task_req> req =
			std::dynamic_pointer_cast<matrix::service_core::node_start_task_req>(msg->get_content());
		if (req == nullptr) return E_DEFAULT;

		//防止同一个msg不停的重复被广播进入同一个节点，导致广播风暴
		if (!check_nonce(req->header.nonce)) {
			LOG_ERROR << "ai power provider service nonce error ";
			return E_DEFAULT;
		}

		if (!dbc::check_id(req->body.task_id)) {
			LOG_ERROR << "ai power provider service task_id error ";
			return E_DEFAULT;
		}

		std::string sign_msg = req->body.task_id + req->header.nonce + req->body.additional;
		if (req->header.exten_info.size() < 3) {
			LOG_ERROR << "exten info error.";
			return E_DEFAULT;
		}
		if (!dbc::verify_sign(req->header.exten_info["sign"], sign_msg, req->header.exten_info["origin_id"])) {
			LOG_ERROR << "sign error." << req->header.exten_info["origin_id"];
			return E_DEFAULT;
		}

		// 检查是否命中当前节点
		const std::vector<std::string>& peer_nodes = req->body.peer_nodes_list;
		bool hit_self = hit_node(peer_nodes, CONF_MANAGER->get_node_id());
		if (hit_self) {
			return task_start(req->body.task_id);
		}
		else {
			CONNECTION_MANAGER->broadcast_message(msg, msg->header.src_sid);
			return E_SUCCESS;
		}
	}

	int32_t node_request_service::on_node_stop_task_req(std::shared_ptr<dbc::network::message>& msg) {
		std::shared_ptr<matrix::service_core::node_stop_task_req> req = std::dynamic_pointer_cast<matrix::service_core::node_stop_task_req>(
			msg->get_content());
		if (req == nullptr) return E_DEFAULT;

		//防止同一个msg不停的重复被广播进入同一个节点，导致广播风暴
		if (!check_nonce(req->header.nonce)) {
			LOG_ERROR << "ai power provider service nonce error ";
			return E_DEFAULT;
		}

		if (!dbc::check_id(req->body.task_id)) {
			LOG_ERROR << "ai power provider service on_stop_training_req task_id error ";
			return E_DEFAULT;
		}

		std::string sign_msg = req->body.task_id + req->header.nonce + req->body.additional;
		if (req->header.exten_info.size() < 3) {
			LOG_ERROR << "exten info error.";
			return E_DEFAULT;
		}
		if (!dbc::verify_sign(req->header.exten_info["sign"], sign_msg, req->header.exten_info["origin_id"])) {
			LOG_ERROR << "sign error." << req->header.exten_info["origin_id"];
			return E_DEFAULT;
		}

		// 检查是否命中当前节点
		const std::vector<std::string>& peer_nodes = req->body.peer_nodes_list;
		bool hit_self = hit_node(peer_nodes, CONF_MANAGER->get_node_id());
		if (hit_self) {
			const std::string& task_id = req->body.task_id;
			return task_stop(task_id);
		}
		else {
			CONNECTION_MANAGER->broadcast_message(msg, msg->header.src_sid);
			return E_SUCCESS;
		}
	}

	int32_t node_request_service::on_node_restart_task_req(std::shared_ptr<dbc::network::message>& msg) {
		std::shared_ptr<matrix::service_core::node_restart_task_req> req =
			std::dynamic_pointer_cast<matrix::service_core::node_restart_task_req>(msg->get_content());
		if (req == nullptr) return E_DEFAULT;

		//防止同一个msg不停的重复被广播进入同一个节点，导致广播风暴
		if (!check_nonce(req->header.nonce)) {
			LOG_ERROR << "ai power provider service nonce error ";
			return E_DEFAULT;
		}

		if (!dbc::check_id(req->body.task_id)) {
			LOG_ERROR << "ai power provider service task_id error ";
			return E_DEFAULT;
		}

		std::string sign_msg = req->body.task_id + req->header.nonce + req->body.additional;
		if (req->header.exten_info.size() < 3) {
			LOG_ERROR << "exten info error.";
			return E_DEFAULT;
		}
		if (!dbc::verify_sign(req->header.exten_info["sign"], sign_msg, req->header.exten_info["origin_id"])) {
			LOG_ERROR << "sign error." << req->header.exten_info["origin_id"];
			return E_DEFAULT;
		}

		// 检查是否命中当前节点
		const std::vector<std::string>& peer_nodes = req->body.peer_nodes_list;
		bool hit_self = hit_node(peer_nodes, CONF_MANAGER->get_node_id());
		if (hit_self) {
			return task_restart(req->body.task_id);
		}
		else {
			CONNECTION_MANAGER->broadcast_message(msg, msg->header.src_sid);
			return E_SUCCESS;
		}
	}

	int32_t node_request_service::on_node_task_logs_req(const std::shared_ptr<dbc::network::message>& msg) {
		std::shared_ptr<node_task_logs_req> req_content = std::dynamic_pointer_cast<node_task_logs_req>(msg->get_content());
		if (req_content == nullptr) return E_DEFAULT;

		//防止同一个msg不停的重复被广播进入同一个节点，导致广播风暴
		if (!check_nonce(req_content->header.nonce)) {
			LOG_ERROR << "ai power provider service nonce error ";
			return E_DEFAULT;
		}

		if (!dbc::check_id(req_content->header.session_id)) {
			LOG_ERROR << "ai power provider service session_id error ";
			return E_DEFAULT;
		}

		if (!dbc::check_id(req_content->body.task_id)) {
			LOG_ERROR << "taskid error ";
			return E_DEFAULT;
		}

		const std::string& task_id = req_content->body.task_id;

		if (GET_LOG_HEAD != req_content->body.head_or_tail && GET_LOG_TAIL != req_content->body.head_or_tail) {
			LOG_ERROR << "ai power provider service on logs req log direction error: " << task_id;
			return E_DEFAULT;
		}

		if (req_content->body.number_of_lines > MAX_NUMBER_OF_LINES || req_content->body.number_of_lines < 0) {
			LOG_ERROR << "ai power provider service on logs req number of lines error: "
				<< req_content->body.number_of_lines;
			return E_DEFAULT;
		}

		std::string sign_req_msg =
			req_content->body.task_id + req_content->header.nonce + req_content->header.session_id +
			req_content->body.additional;
		if (!dbc::verify_sign(req_content->header.exten_info["sign"], sign_req_msg, req_content->header.exten_info["origin_id"])) {
			LOG_ERROR << "fake message. " << req_content->header.exten_info["origin_id"];
			return E_DEFAULT;
		}

		// 检查是否命中当前节点
		const std::vector<std::string>& peer_nodes = req_content->body.peer_nodes_list;
		bool hit_self = hit_node(peer_nodes, CONF_MANAGER->get_node_id());
		if (hit_self) {
			return task_logs(req_content);
		}
		else {
			req_content->header.path.push_back(CONF_MANAGER->get_node_id());
			CONNECTION_MANAGER->broadcast_message(msg, msg->header.src_sid);
			return E_SUCCESS;
		}
	}

	int32_t node_request_service::on_node_list_task_req(std::shared_ptr<dbc::network::message>& msg) {
		std::shared_ptr<node_list_task_req> req_content = std::dynamic_pointer_cast<node_list_task_req>(
			msg->get_content());
		if (req_content == nullptr) return E_DEFAULT;

		//防止同一个msg不停的重复被广播进入同一个节点，导致广播风暴
		if (!check_nonce(req_content->header.nonce)) {
			LOG_ERROR << "ai power provider service nonce error ";
			return E_DEFAULT;
		}

		if (!dbc::check_id(req_content->header.session_id)) {
			LOG_ERROR << "ai power provider service sessionid error ";
			return E_DEFAULT;
		}

		if (!req_content->body.task_id.empty() && !dbc::check_id(req_content->body.task_id)) {
			LOG_ERROR << "ai power provider service taskid error: " << req_content->body.task_id;
			return E_DEFAULT;
		}

		std::string sign_msg = req_content->body.task_id + req_content->header.nonce +
			req_content->header.session_id + req_content->body.additional;
		if (!dbc::verify_sign(req_content->header.exten_info["sign"], sign_msg, req_content->header.exten_info["origin_id"])) {
			LOG_ERROR << "fake message. " << req_content->header.exten_info["origin_id"];
			return E_DEFAULT;
		}

		// 检查是否命中当前节点
		const std::vector<std::string>& peer_nodes = req_content->body.peer_nodes_list;
		bool hit_self = hit_node(peer_nodes, CONF_MANAGER->get_node_id());
		if (!hit_self) {
			req_content->header.path.push_back(CONF_MANAGER->get_node_id());
			CONNECTION_MANAGER->broadcast_message(msg, msg->header.src_sid);
			return E_SUCCESS;
		}
		else {
            std::vector<matrix::service_core::task_status> status_list;

			if (req_content->body.task_id.empty()) {
                std::vector<std::shared_ptr<TaskInfo>> task_list;
				m_task_scheduler.ListAllTask(task_list);
				for (auto &task : task_list) {
                    matrix::service_core::task_status ts;
                    ts.__set_task_id(task->task_id);
                    ts.__set_status(m_task_scheduler.GetTaskStatus(task->task_id));
                    ts.__set_pwd(task->login_password);
                    status_list.push_back(ts);
				}
			}
			else {
				auto task = m_task_scheduler.FindTask(req_content->body.task_id);
				if (nullptr != task) {
					matrix::service_core::task_status ts;
					ts.__set_task_id(task->task_id);
					ts.__set_status(m_task_scheduler.GetTaskStatus(task->task_id));
					ts.__set_pwd(task->login_password);
					status_list.push_back(ts);
				}
			}

            std::shared_ptr<matrix::service_core::node_list_task_rsp> rsp_content =
                    std::make_shared<matrix::service_core::node_list_task_rsp>();

            rsp_content->header.__set_magic(CONF_MANAGER->get_net_flag());
            rsp_content->header.__set_msg_name(NODE_LIST_TASK_RSP);
            rsp_content->header.__set_nonce(dbc::create_nonce());
            rsp_content->header.__set_session_id(req_content->header.session_id);
            rsp_content->header.__set_path(req_content->header.path);
            rsp_content->body.task_status_list.swap(status_list);

            std::string task_status_msg;
            for (auto& ts : status_list) {
                task_status_msg = task_status_msg + ts.task_id + boost::str(boost::format("%d") % ts.status);
            }
            std::string sign_msg = rsp_content->header.nonce + rsp_content->header.session_id + task_status_msg;
            std::map<std::string, std::string> exten_info;
            exten_info["origin_id"] = CONF_MANAGER->get_node_id();
            std::string sign = dbc::sign(sign_msg, CONF_MANAGER->get_node_private_key());
            exten_info["sign"] = sign;
            exten_info["sign_algo"] = ECDSA;
            time_t cur = std::time(nullptr);
            exten_info["sign_at"] = boost::str(boost::format("%d") % cur);

            rsp_content->header.__set_exten_info(exten_info);

            //rsp msg
            std::shared_ptr<dbc::network::message> resp_msg = std::make_shared<dbc::network::message>();
            resp_msg->set_name(NODE_LIST_TASK_RSP);
            resp_msg->set_content(rsp_content);
            CONNECTION_MANAGER->send_resp_message(resp_msg);

			return E_SUCCESS;
		}
	}

    int32_t node_request_service::on_get_task_queue_size_req(std::shared_ptr<dbc::network::message>& msg) {
        auto resp = std::make_shared<get_task_queue_size_resp_msg>();

        /*
        auto task_num = m_task_scheduler.get_user_cur_task_size();
        resp->set_task_size(task_num);
        resp->set_gpu_state(m_task_scheduler.get_gpu_state());
        resp->set_active_tasks(m_task_scheduler.get_active_tasks());
        */

        auto resp_msg = std::dynamic_pointer_cast<dbc::network::message>(resp);

        TOPIC_MANAGER->publish<int32_t>(typeid(get_task_queue_size_resp_msg).name(), resp_msg);

        return E_SUCCESS;
    }

	int32_t node_request_service::task_create(const std::shared_ptr<matrix::service_core::node_create_task_req>& req) {
		if (req == nullptr) return E_DEFAULT;
		return m_task_scheduler.CreateTask(req->body.task_id, req->body.additional);
	}

	int32_t node_request_service::task_start(const std::string& task_id) {
        return m_task_scheduler.StartTask(task_id);
	}

	int32_t node_request_service::task_stop(const std::string& task_id) {
	    return m_task_scheduler.StopTask(task_id);
	}

	int32_t node_request_service::task_restart(const std::string& task_id) {
        return m_task_scheduler.RestartTask(task_id);
	}

	int32_t node_request_service::task_logs(const std::shared_ptr<matrix::service_core::node_task_logs_req>& req) {
		std::string log_content = m_task_scheduler.GetTaskLog(req->body.task_id, (ETaskLogDirection) req->body.head_or_tail, req->body.number_of_lines);
	  
		std::shared_ptr<matrix::service_core::node_task_logs_rsp> rsp_content = std::make_shared<matrix::service_core::node_task_logs_rsp>();
		rsp_content->header.__set_magic(CONF_MANAGER->get_net_flag());
		rsp_content->header.__set_msg_name(NODE_TASK_LOGS_RSP);
		rsp_content->header.__set_nonce(dbc::create_nonce());
		rsp_content->header.__set_session_id(req->header.session_id);
		rsp_content->header.__set_path(req->header.path);

		peer_node_log log;
		log.__set_peer_node_id(CONF_MANAGER->get_node_id());
		log.__set_log_content(log_content);
 
		if (GET_LOG_HEAD == req->body.head_or_tail) {
			log.log_content = log.log_content.substr(0, MAX_LOG_CONTENT_SIZE);
		}
		else {
			size_t log_lenth = log.log_content.length();
			if (log_lenth > MAX_LOG_CONTENT_SIZE) {
				log.log_content = log.log_content.substr(log_lenth - MAX_LOG_CONTENT_SIZE, MAX_LOG_CONTENT_SIZE);
			}
		}

		rsp_content->body.__set_log(log);
		
		// sign
		std::map<std::string, std::string> exten_info;

		std::string sign_msg =
			CONF_MANAGER->get_node_id() + rsp_content->header.nonce + rsp_content->header.session_id +
			rsp_content->body.log.log_content;

        std::string sign = dbc::sign(sign_msg, CONF_MANAGER->get_node_private_key());
        exten_info["sign"] = sign;
        exten_info["sign_algo"] = ECDSA;
        time_t cur = std::time(nullptr);
        exten_info["sign_at"] = boost::str(boost::format("%d") % cur);
        exten_info["origin_id"] = CONF_MANAGER->get_node_id();
		rsp_content->header.__set_exten_info(exten_info);

		//resp msg
		std::shared_ptr<dbc::network::message> resp_msg = std::make_shared<dbc::network::message>();
		resp_msg->set_name(NODE_TASK_LOGS_RSP);
		resp_msg->set_content(rsp_content);
		CONNECTION_MANAGER->send_resp_message(resp_msg);

		return E_SUCCESS;
	}

	int32_t node_request_service::on_training_task_timer(const std::shared_ptr<core_timer>& timer) {
	    m_task_scheduler.ProcessTask();

		static int count = 0;
		count++;
		if ((count % 10) == 0) {
			//m_task_schedule->update_gpu_info_from_proc();
		}

		return E_SUCCESS;
	}

    int32_t node_request_service::on_prune_task_timer(const std::shared_ptr<core_timer>& timer) {
	    m_task_scheduler.PruneTask();
	    return E_SUCCESS;
	}

    int32_t node_request_service::check_sign(const std::string& message, const std::string& sign,
                                             const std::string& origin_id, const std::string& sign_algo) {
        if (sign_algo != ECDSA) {
            LOG_ERROR << "sign_algorithm error.";
            return E_DEFAULT;
        }

        if (origin_id.empty() || sign.empty()) {
            LOG_ERROR << "sign error.";
            return E_DEFAULT;
        }

        return E_SUCCESS;
    }

    std::string node_request_service::get_task_id(const std::string& server_specification) {
        if (server_specification.empty()) {
            return "";
        }

        std::string task_id;
        if (!server_specification.empty()) {
            std::stringstream ss;
            ss << server_specification;
            boost::property_tree::ptree pt;

            try {
                boost::property_tree::read_json(ss, pt);
                if (pt.count("task_id") != 0) {
                    task_id = pt.get<std::string>("task_id");
                }
            }
            catch (...) {

            }
        }

        return task_id;
    }

    std::string node_request_service::format_logs(const std::string& raw_logs, uint16_t max_lines) {
        //docker logs has special format with each line of log:
        // 0x01 0x00  0x00 0x00 0x00 0x00 0x00 0x38
        //we should remove it
        //and ends with 0x30 0x0d 0x0a
        max_lines = (max_lines == 0) ? MAX_NUMBER_OF_LINES : max_lines;
        size_t size = raw_logs.size();
        vector<unsigned char> log_vector(size);

        int push_char_count = 0;
        const char* p = raw_logs.c_str();

        uint16_t line_count = 1;

        for (size_t i = 0; i < size;) {
            //0x30 0x0d 0x0a
            if ((i + 2 < size)
                && (0x30 == *p)
                && (0x0d == *(p + 1))
                && (0x0a == *(p + 2))) {
                break;
            }

            if (max_lines != 0 && line_count > max_lines) {
                break;
            }

            //skip: 0x01 0x00  0x00 0x00 0x00 0x00 0x00 0x38
            if ((i + 7 < size)
                && ((0x01 == *p) || (0x02 == *p))
                && (0x00 == *(p + 1))
                && (0x00 == *(p + 2))
                && (0x00 == *(p + 3))
                && (0x00 == *(p + 4))
                && (0x00 == *(p + 5))) {
                i += 8;
                p += 8;
                continue;
            }

            log_vector[push_char_count] = *p++;

            if (log_vector[push_char_count] == '\n') {
                line_count++;
            }

            ++push_char_count;
            i++;
        }

        std::string formatted_str;
        formatted_str.reserve(push_char_count);

        int i = 0;
        while (i < push_char_count) {
            formatted_str += log_vector[i++];
        }

        return formatted_str;
    }
}

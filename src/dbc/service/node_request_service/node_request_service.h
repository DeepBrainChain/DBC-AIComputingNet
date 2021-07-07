#ifndef DBC_NODE_REQUEST_SERVICE_H
#define DBC_NODE_REQUEST_SERVICE_H

#include <leveldb/db.h>
#include <string>
#include "service_module.h"
#include "container_client.h"
#include "task_common_def.h"
#include "document.h"
#include "oss_client.h"
#include <boost/process.hpp>
#include "image_manager.h"
#include "task_scheduler.h"
#include "LruCache.hpp"

#define AI_TRAINING_TASK_TIMER                                   "training_task"
//#define AI_TRAINING_TASK_TIMER_INTERVAL                        (30 * 1000)                                                 //30s timer
#define AI_PRUNE_TASK_TIMER                                      "prune_task"
#define AI_PRUNE_TASK_TIMER_INTERVAL                             (10 * 60 * 1000)                                                 //10min timer

using namespace matrix::core;
namespace bp = boost::process;

namespace dbc {
	class node_request_service : public service_module {
	public:
		node_request_service();

		~node_request_service() override = default;

		std::string module_name() const override { return "node_request_service"; }

	protected:
		void init_timer() override;

		void init_invoker() override;

		void init_subscription() override;

		int32_t service_init(bpo::variables_map& options) override;

		int32_t service_exit() override;

		int32_t on_node_create_task_req(std::shared_ptr<dbc::network::message>& msg);

		int32_t on_node_start_task_req(std::shared_ptr<dbc::network::message>& msg);

		int32_t on_node_restart_task_req(std::shared_ptr<dbc::network::message>& msg);

		int32_t on_node_stop_task_req(std::shared_ptr<dbc::network::message>& msg);

        int32_t on_node_reset_task_req(std::shared_ptr<dbc::network::message>& msg);

        int32_t on_node_destroy_task_req(std::shared_ptr<dbc::network::message>& msg);

        int32_t on_node_task_logs_req(const std::shared_ptr<dbc::network::message>& msg);

		int32_t on_node_list_task_req(std::shared_ptr<dbc::network::message>& msg);

		int32_t task_create(const std::shared_ptr<matrix::service_core::node_create_task_req>& req);

		int32_t task_start(const std::shared_ptr<matrix::service_core::node_start_task_req>& req);

		int32_t task_stop(const std::shared_ptr<matrix::service_core::node_stop_task_req>& req);

        int32_t task_restart(const std::shared_ptr<matrix::service_core::node_restart_task_req>& req);

        int32_t task_reset(const std::shared_ptr<matrix::service_core::node_reset_task_req>& req);

        int32_t task_destroy(const std::shared_ptr<matrix::service_core::node_destroy_task_req>& req);

		int32_t task_logs(const std::shared_ptr<matrix::service_core::node_task_logs_req>& req);

		int32_t on_get_task_queue_size_req(std::shared_ptr<dbc::network::message>& msg);

		int32_t on_training_task_timer(const std::shared_ptr<core_timer>& timer);

		int32_t on_prune_task_timer(const std::shared_ptr<core_timer>& timer);

		std::string format_logs(const std::string& raw_logs, uint16_t max_lines);

		int32_t check_sign(const std::string& message, const std::string& sign, const std::string& origin_id,
			const std::string& sign_algo);

		std::string get_task_id(const std::string& server_specification);

	private:
		bool check_nonce(const std::string& nonce);
		bool hit_node(const std::vector<std::string>& peer_node_list, const std::string& node_id);

	protected:
		TaskScheduler m_task_scheduler;

		uint32_t m_training_task_timer_id = INVALID_TIMER_ID;
		uint32_t m_prune_task_timer_id = INVALID_TIMER_ID;

		lru::Cache<std::string, int32_t, std::mutex> m_nonceCache{ 1000000, 0 };
	};
}

#endif

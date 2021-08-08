#ifndef DBC_CONF_MANAGER_H
#define DBC_CONF_MANAGER_H

#include "util/utils.h"
#include "module/module.h"
#include "net_type_params.h"
#include "network/compress/matrix_capacity.h"

using namespace boost::program_options;

static const std::string DEFAULT_VM_LISTEN_PORT = "16509";
static const std::string DEFAULT_CONTAINER_LISTEN_PORT = "31107";
static const std::string DEFAULT_REST_PORT = "41107";
static const std::string DEFAULT_CONTAINER_IMAGE_NAME = "dbctraining/tensorflow-cpu-0.1.0:v1";
static const int32_t DEFAULT_MAX_CONNECTION_NUM = 512;
static const int32_t DEFAULT_TIMER_DBC_REQUEST_IN_SECOND = 20;
static const int32_t DEFAULT_TIMER_SERVICE_BROADCAST_IN_SECOND = 30;
static const int32_t DEFAULT_TIMER_SERVICE_LIST_EXPIRED_IN_SECOND = 300;
static const int32_t DEFAULT_SPEED = 0;
static const bool DEFAULT_ENABLE = true;
static const bool DEFAULT_DISABLE = false;
static const int32_t DEFAULT_UPDATE_IDLE_TASK_CYCLE = 24*60;   //24h
static const int32_t DEFAULT_TIMER_AI_TRAINING_TASK_SCHEDULE_IN_SECOND = 15;
static const int32_t DEFAULT_TIMER_LOG_REFRESH_IN_SECOND = 5;
static const int64_t DEFAULT_USE_SIGN_TIME = 0x7FFFFFFFFFFFFFFF;
static const int16_t DEFAULT_PRUNE_CONTAINER_INTERVAL=240; //48 hours,test:1 hours
static const int16_t DEFAULT_PRUNE_TASK_INTERVAL=2400;     //100 days
static const int16_t DEFALUT_PRUNE_DOCKER_ROOT_USE_RATIO=5;
static const std::vector<std::string> DEFAULT_VECTOR = std::vector<std::string>();

#define NODE_FILE_NAME                               "node.dat"
#define DEFAULT_MAIN_NET_LISTEN_PORT                 "11118"
#define DEFAULT_TEST_NET_LISTEN_PORT                 "21107"

class conf_manager : public Singleton<conf_manager>
{
public:
    conf_manager();

    virtual ~conf_manager() = default;

    ERR_CODE init(bpo::variables_map &options);

    int32_t get_log_level() const { return m_log_level; }

    int32_t get_net_flag() const { return m_net_flag; }

    const std::string & get_net_type() { return m_net_type; }

    uint16_t get_net_default_port();

    const std::string & get_net_listen_port() { return m_net_params->get_net_listen_port(); }

    const std::string & get_host_ip() {return m_args.count("host_ip") ? m_args["host_ip"].as<std::string>() : DEFAULT_IP_V4;}

    const std::vector<std::string> & get_peers() { return m_args.count("peer") ? m_args["peer"].as<std::vector<std::string>>() : DEFAULT_VECTOR; }

    const std::vector<const char *> &get_dns_seeds() { return m_net_params->get_dns_seeds(); }

    const std::vector<peer_seeds> &get_hard_code_seeds() { return m_net_params->get_hard_code_seeds(); }

    const std::string & get_container_ip() { return m_args.count("container_ip") ? m_args["container_ip"].as<std::string>() : DEFAULT_LOCAL_IP; }

    const std::string & get_container_port() { return m_args.count("container_port") ? m_args["container_port"].as<std::string>() : DEFAULT_CONTAINER_LISTEN_PORT; }

    const std::string & get_vm_ip() { return m_args.count("virt_ip") ? m_args["virt_ip"].as<std::string>() : DEFAULT_LOCAL_IP; }

    const std::string & get_vm_port() { return m_args.count("virt_port") ? m_args["virt_port"].as<std::string>() : DEFAULT_VM_LISTEN_PORT; }

    const int32_t & get_max_connect() {return m_args.count("max_connect") ? m_args["max_connect"].as<int32_t>() : DEFAULT_MAX_CONNECTION_NUM;}

    int32_t get_timer_service_broadcast_in_second() {
        return m_args.count("timer_service_broadcast_in_second") ? m_args["timer_service_broadcast_in_second"].as<int32_t>() : DEFAULT_TIMER_SERVICE_BROADCAST_IN_SECOND;
    }

    int32_t get_timer_service_list_expired_in_second() {
        return m_args.count("timer_service_list_expired_in_second") ? m_args["timer_service_list_expired_in_second"].as<int32_t>() : DEFAULT_TIMER_SERVICE_LIST_EXPIRED_IN_SECOND;
    }

    int32_t  get_timer_dbc_request_in_millisecond() {
        if( m_args.count("timer_dbc_request_in_second"))
            return (m_args["timer_dbc_request_in_second"].as<int32_t>()) * 1000;
        else
            return DEFAULT_TIMER_DBC_REQUEST_IN_SECOND * 1000;
    }

    int32_t get_timer_ai_training_task_schedule_in_second() {
        return m_args.count("timer_ai_training_task_schedule_in_second") ? m_args["timer_ai_training_task_schedule_in_second"].as<int32_t>() : DEFAULT_TIMER_AI_TRAINING_TASK_SCHEDULE_IN_SECOND;
    }

    int32_t get_timer_log_refresh_in_second() {
        return m_args.count("timer_log_refresh_in_second") ? m_args["timer_log_refresh_in_second"].as<int32_t>() : DEFAULT_TIMER_LOG_REFRESH_IN_SECOND;
    }

    const std::string & get_node_id() const { return m_node_id; }

    const std::string & get_node_private_key() const {return m_node_private_key;}

    static int32_t serialize_node_info(const util::machine_node_info &info);

    const std::string & get_oss_url()
    {
        return  m_args.count("oss_url") > 0 ?  m_args["oss_url"].as<std::string>():DEFAULT_STRING;
    }

    const std::string & get_oss_crt()
    {
        return m_args.count("oss_crt") > 0 ? m_args["oss_crt"].as<std::string>() : DEFAULT_STRING;
    }

    const int32_t & get_max_recv_speed()
    {
        return m_args.count("max_recv_speed") > 0 ? m_args["max_recv_speed"].as<int32_t>() : DEFAULT_SPEED;
    }

    const std::string& get_rest_ip()
    {
        return m_args.count("rest_ip") ? m_args["rest_ip"].as<std::string>() : DEFAULT_LOOPBACK_IP;
    }

    const std::string& get_rest_port()
    {
        return m_args.count("rest_port") ? m_args["rest_port"].as<std::string>() : DEFAULT_REST_PORT;
    }

    const bool & get_enable_idle_task() { return m_args.count("enable_idle_task") > 0 ? m_args["enable_idle_task"].as<bool>() : DEFAULT_ENABLE; }
    const bool & get_enable_node_reboot() { return m_args.count("enable_node_reboot") > 0 ? m_args["enable_node_reboot"].as<bool>() : DEFAULT_DISABLE; }
    const bool & get_enable_billing() { return m_args.count("enable_billing") > 0 ? m_args["enable_billing"].as<bool>() : DEFAULT_ENABLE; }
    const int32_t & get_update_idle_task_cycle() { return m_args.count("update_idle_task_cycle") > 0 ? m_args["update_idle_task_cycle"].as<int32_t>() : DEFAULT_UPDATE_IDLE_TASK_CYCLE; }
    const dbc::network::matrix_capacity & get_proto_capacity()
    {
        return m_proto_capacity;
    }

    const int64_t & get_use_sign_time() { return m_args.count("use_sign_time") > 0 ? m_args["use_sign_time"].as<int64_t>() : DEFAULT_USE_SIGN_TIME;}
    const int16_t & get_prune_container_stop_interval() {return m_args.count("prune_container_stop_interval") > 0 ? m_args["prune_container_stop_interval"].as<int16_t>() : DEFAULT_PRUNE_CONTAINER_INTERVAL;}
    const int16_t & get_prune_docker_root_use_ratio() {return m_args.count("prune_docker_root_use_ratio") > 0 ? m_args["prune_docker_root_use_ratio"].as<int16_t>() : DEFALUT_PRUNE_DOCKER_ROOT_USE_RATIO;}
    const int16_t & get_prune_task_stop_interval() {return m_args.count("prune_task_stop_interval") > 0 ? m_args["prune_task_stop_interval"].as<int16_t>() : DEFAULT_PRUNE_TASK_INTERVAL;}

    std::string get_auth_mode()
    {
        return  m_args.count("auth_mode") > 0 ?  m_args["auth_mode"].as<std::string>(): std::string("online");
    }

    std::vector<std::string>  get_trust_node_ids()
    {
        if ( m_args.count("trust_node_id") == 0 )
        {
            std::vector<std::string> rtn;
            return rtn;
        }

        return m_args["trust_node_id"].as< std::vector<std::string> >();
    }

    std::string get_dbc_chain_domain() const {
        return m_args["dbc_chain_domain"].as<std::string>();
    }

protected:
    int32_t parse_local_conf();

    int32_t parse_node_dat();

    int32_t init_params();

    const variable_value& operator[](const std::string& name) const { return m_args[name]; }

    bool count(const std::string& name) const { return m_args.count(name);}

    int32_t gen_new_nodeid();

    bool check_node_info();

private:
    variables_map m_args;

    std::string m_node_id;
    std::string m_node_private_key;

    std::string m_net_type;
    int32_t m_net_flag;

    int32_t m_log_level = 2;

    //params relative with net type
    std::shared_ptr<net_type_params> m_net_params;

    //protocol capacity
    dbc::network::matrix_capacity m_proto_capacity;
};

#endif

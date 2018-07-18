#include "comm_def.h"
#include "peers_db_types.h"
using namespace matrix::service_core;

std::string to_node_type_string(int8_t status)
{
    switch (status)
    {
    case NORMAL_NODE:
        return "NORMAL_NODE";
    case SEED_NODE:
        return "SEED_NODE";
    default:
        return "";
    }
}


std::string to_training_task_status_string(int8_t status)
{
    switch (status)
    {
    case task_unknown:
        return "task_unknown";
    case task_queueing:
        return "task_queueing";
    case task_running:
        return "task_running";
    case task_stopped:
        return "task_stopped";
    case task_succefully_closed:
        return "task_succefully_closed";
    case task_abnormally_closed:
        return "task_abnormally_closed";
    default:
        return "";
    }
}

std::string to_net_state_string(int8_t status)
{
    switch (status)
    {
    case ns_idle:
        return "idle";
    case ns_in_use:
        return "in_use";
    case ns_failed:
        return "failed";
    case ns_zombie:
        return "zombie";
    case ns_available:
        return "available";
    default:
        return "";
    }
}

#include "def.h"
#include "util/utils.h"

std::string task_operation_string(int32_t op) {
    std::string operation_string = "none";
    switch (op) {
    case T_OP_Create:
        operation_string = "create";
        break;
    case T_OP_Start:
        operation_string = "start";
        break;
    case T_OP_Stop:
        operation_string = "stop";
        break;
    case T_OP_ReStart:
        operation_string = "restart";
        break;
    case T_OP_Reset:
        operation_string = "reset";
        break;
    case T_OP_Delete:
        operation_string = "delete";
        break;
    case T_OP_CreateSnapshot:
        operation_string = "create snapshot";
        break;
    default:
        break;
    }

    return operation_string;
}

std::string task_status_string(int32_t status) {
    std::string status_string = "closed";
    switch (status) {
    case TS_ShutOff:
        status_string = "closed";
        break;
    case TS_Creating:
        status_string = "creating";
        break;
    case TS_Running:
        status_string = "running";
        break;
    case TS_Starting:
        status_string = "starting";
        break;
    case TS_Stopping:
        status_string = "stopping";
        break;
    case TS_Restarting:
        status_string = "restarting";
        break;
    case TS_Resetting:
        status_string = "resetting";
        break;
    case TS_Deleting:
        status_string = "deleting";
        break;
    case TS_CreatingSnapshot:
        status_string = "creating snapshot";
        break;
    case TS_PMSuspended:
        status_string = "pm_suspended";
        break;
    case TS_Error:
        status_string = "error";
        break;
    case TS_CreateError:
        status_string = "create error";
        break;
    case TS_StartError:
        status_string = "start error";
        break;
    case TS_RestartError:
        status_string = "restart error";
        break;
    case TS_ResetError:
        status_string = "reset error";
        break;
    case TS_DeleteError:
        status_string = "delete error";
        break;
    case TS_CreateSnapshotError:
        status_string = "create snapshot error";
        break;
    default:
        break;
    }

    return status_string;
}

std::string vm_status_string(virDomainState status) {
    std::string status_string = "none";
    switch (status) {
    case VIR_DOMAIN_NOSTATE:
        status_string = "none";
        break;
    case VIR_DOMAIN_PAUSED:
        status_string = "paused";
        break;
    case VIR_DOMAIN_RUNNING:
        status_string = "running";
        break;
    case VIR_DOMAIN_SHUTOFF:
        status_string = "shut_off";
        break;
    case VIR_DOMAIN_BLOCKED:
        status_string = "blocked";
        break;
    case VIR_DOMAIN_SHUTDOWN:
        status_string = "being shutdown";
        break;
    case VIR_DOMAIN_CRASHED:
        status_string = "crashed";
        break;
    case VIR_DOMAIN_PMSUSPENDED:
        status_string = "suspended";
        break;
    default:
        break;
    }

    return status_string;
}

std::string ImageServer::to_string() {
    return id + "," + ip + "," + port + "," + modulename;
}

void ImageServer::from_string(const std::string& str) {
    std::vector<std::string> val = util::split(str, ",");
    if (val.size() >= 1) {
        this->id = val[0];
    }

    if (val.size() >= 2) {
        this->ip = val[1];
    }

    if (val.size() >= 3) {
        this->port = val[2];
    }

    if (val.size() >= 4) {
        this->modulename = val[3];
    }
}

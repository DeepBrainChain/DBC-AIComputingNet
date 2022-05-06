#include "def.h"
#include "util/utils.h"

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

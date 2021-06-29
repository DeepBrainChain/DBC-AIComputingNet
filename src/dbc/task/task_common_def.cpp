#include "task_common_def.h"
#include <boost/xpressive/xpressive_dynamic.hpp>  

namespace dbc {
    using namespace boost::xpressive;

    std::string to_training_task_status_string(int8_t status) {
        switch (status) {
            case task_status_unknown:
                return "task_status_unknown";
            case task_status_queueing:
                return "task_status_queueing";
            case task_status_running: {
                //std::string task_state = { 0x1b,0x5b,0x33,0x32,0x6d};  //green
                //task_state += "task_status_running";
                //task_state += {0x1b,0x5b,0x30,0x6d};
                return "task_status_running";
            }
            case task_status_stopped:
                return "task_status_stopped";
            case task_successfully_closed:
                return "task_successfully_closed";
            case task_abnormally_closed: {
                //std::string task_state = { 0x1b,0x5b,0x33,0x31,0x6d }; //red
                //task_state += "task_abnormally_closed";
                //task_state += {0x1b, 0x5b, 0x30, 0x6d};
                return "task_abnormally_closed";
            }
            case task_overdue_closed:
                return "task_overdue_closed";
            case task_status_pulling_image:
                return "task_status_pulling_image";
            case task_noimage_closed:
                return "task_noimage_closed";
            case task_nospace_closed:
                return "task_nospace_closed";
            case task_status_out_of_gpu_resource:
                return "task_status_out_of_gpu_resource";
            case task_status_update_error:
                return "task_status_update_error";
            case task_status_creating_image:
                return "task_status_creating_image";
            default:
                return DEFAULT_STRING;
        }
    }

    std::string engine_reg;

    bool check_task_engine(const std::string &engine) {
        if (engine_reg.empty()) {
            return engine.length() != 0;
        }
        try {
            cregex reg = cregex::compile(engine_reg);
            return regex_match(engine.c_str(), reg);
        }
        catch (...) {
            return false;
        }

        return false;
    }

    void set_task_engine(std::string engine) {
        engine_reg = engine;
    }

}

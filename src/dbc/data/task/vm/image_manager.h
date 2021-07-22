#ifndef DBC_IMAGE_MANAGER_H
#define DBC_IMAGE_MANAGER_H

#include "util/utils.h"
#include <boost/process.hpp>

namespace bp = boost::process;

// 镜像拉取时的状态
enum pull_state
{
    PREPARE,
    PULLING,
    PULLING_SUCCESS,
    PULLING_ERROR
};

enum create_state
{
    PREPAREING,
    CREATING,
    CREATE_SUCCESS,
    CREATE_ERROR
};

class image_manager
{
public:

    image_manager();

    ~image_manager() = default;

    int32_t start_pull(const std::string & image_name);

    int32_t terminate_pull();
    pull_state check_pull_state();

    int64_t get_start_time() { return m_start_time; }
    void set_start_time(uint64_t start_time) {m_start_time = start_time;}
    std::string get_out_log(const std::string & image_name);
    std::string get_pull_image_name() { return m_image_name;}

 //   int32_t start_create(const std::string & image_name);
 //   pull_state check_creating_state();

private:
    std::shared_ptr<bp::child> m_c_pull_image;
    int64_t m_start_time = 0;
    std::set<std::string> m_pulling_tasks;
    std::string m_image_name="";
 //   std::shared_ptr<bp::child> m_c_create_image;
 //   std::set<std::string> m_creating_tasks;
};

#endif

#ifndef DBC_NODE_INFO_COLLECTION_H
#define DBC_NODE_INFO_COLLECTION_H

#include "util/utils.h"
#include <boost/serialization/singleton.hpp>
#include <ctime>

using namespace boost::serialization;

class node_info_collection
{
public:
    node_info_collection();

    int32_t init(const std::string& filename);

    void refresh();

    std::string get(const std::string& key);

    void set(const std::string& key, const std::string& value);

    std::vector<std::string> get_all_attributes();

private:
    bool init_dbc_bash_sh(const std::string& filename, const std::string& content);

    void generate_node_static_info(const std::string& path);

    int32_t read_node_static_info(const std::string& filename);

    std::string shell_get(const std::string& key);
private:
    std::map<std::string, std::string> m_kvs;
    std::string m_dbc_bash_filename;
};

#endif

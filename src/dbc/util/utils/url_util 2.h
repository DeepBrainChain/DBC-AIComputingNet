#ifndef DBC_URL_UTIL_H
#define DBC_URL_UTIL_H

#include <iostream>
#include <string>
#include <vector>
#include <map>

namespace util {
    //  split path(as: /aaa/bbb/ccc?key=val) into path list
    void split_path(const std::string& path, std::vector<std::string>& path_list);

    //  split path(as: /aaa/bbb/ccc?key1=value1&key2=value2)
    //        into query_table: <key1,value1> ;<key2,value2>
    void split_path_into_kvs(const std::string& uri, std::map<std::string, std::string>& query_table);


    //for example:  "total: 47G free: 46G\n"
    //        if given key is "total",then should get value:47G
    void get_value_from_string(const std::string& str, const std::string& key, std::string& val);


    //for example:  "Filesystem      Size  Used Avail"
    //        result should be: [Filesystem,Size,Used,Avail]
    void split_line_to_itemlist(const std::string& str, std::vector<std::string>& item_list);
}

#endif //DBC_URL_UTIL_H

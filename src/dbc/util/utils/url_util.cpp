#include "url_util.h"
#include "string_util.h"

namespace util {
    //  split path(as: /aaa/bbb/ccc?key=val) into path list
    void split_path(const std::string& path, std::vector<std::string>& path_list)
    {
        if(path.empty())
            return;

        std::string temp_path(path);

        size_t pos = path.find_first_of("?");
        if(pos != std::string::npos) {
            temp_path = path.substr(0, pos);
        }

        if(temp_path[0] == '/') {
            temp_path = temp_path.substr(1);
            util::split(temp_path, "/", path_list);
        } else {
            util::split(temp_path, "/", path_list);
        }
    }

    //  split path(as: /aaa/bbb/ccc?key1=value1&key2=value2)
    //        into query_table: <key1,value1> ;<key2,value2>
    void split_path_into_kvs(const std::string& uri, std::map<std::string, std::string>& query_table)
    {
        size_t pos = uri.find_first_of("?");
        if(pos == std::string::npos)
        {
            return;
        }

        std::string query_string = uri.substr(pos + 1);
        if(query_string.empty())
        {
            return;
        }

        std::vector<std::string> item_list;
        util::split(query_string, "&", item_list);
        for(auto& item : item_list)
        {
            std::vector<std::string> kv_list;
            util::split(item, "=", kv_list);

            if(kv_list.size() == 2)
            {
                query_table[kv_list[0]] = kv_list[1];
            }
        }

    }


    //for example:  "total: 47G free: 46G\n"
    //        if given key is "total",then should get value:47G
    void get_value_from_string(const std::string& str, const std::string& key, std::string& val)
    {

        size_t pos = 0;

        size_t index1 = str.find(key + ":", pos);
        if(index1 == std::string::npos)
        {
            return;
        }

        pos = index1 + key.length() + 1;
        size_t index2 = str.find(":", pos);
        if(index2 == std::string::npos)
        {
            val = str.substr(pos);
        }
        else
        {
            size_t index3 = str.rfind(" ", index2);
            if(index3 == std::string::npos)
            {
                return;
            }

            val = str.substr(pos, index3 - pos);
        }
        val = util::rtrim(val, '\n');
        util::trim(val);
    }


    //for example:  "Filesystem      Size  Used Avail"
    //        result should be: [Filesystem,Size,Used,Avail]
    void split_line_to_itemlist(const std::string& str, std::vector<std::string>& item_list)
    {
        static const std::string SPACE = " ";
        item_list.clear();
        util::split(str, SPACE, item_list);

        std::vector<std::string>::iterator it = item_list.begin();
        for(; it != item_list.end();)
        {
            std::string item = *it;
            util::trim(item);

            if(item.empty())
            {
                it = item_list.erase(it);
            }
            else
            {
                ++it;
            }

        }
    }
}

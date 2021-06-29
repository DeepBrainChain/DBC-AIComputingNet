#ifndef DBC_NODE_INFO_MESSAGE_H
#define DBC_NODE_INFO_MESSAGE_H

#include <string>
#include <vector>
#include "matrix_types.h"
#include "service_message.h"

namespace service
{
    namespace misc
    {
        class node_info_query_req_msg
        {
        public:
            node_info_query_req_msg();
            node_info_query_req_msg(std::shared_ptr<message> msg);
            int32_t send();
            void prepare(std::string o_node_id, std::string d_node_id, std::vector<std::string> key,std::string
            session_id);
            std::string get_session_id();

            bool validate();

        private:
            std::shared_ptr<message> m_msg;
        };

        class node_info_query_sessions
        {
        public:
            static void add(std::string session_id);
            static void rm(std::string session_id);
            static bool is_valid_session(std::string session_id);

        private:
            static std::map <std::string, std::string> m_sessions; // outstanding session
        };

        class node_info_query_resp_msg
        {
        public:
            node_info_query_resp_msg(std::shared_ptr<message> msg);
            node_info_query_resp_msg();

            int32_t send();

            int32_t prepare(std::string o_node_id, std::string d_node_id, std::string session_id,
                            std::map<std::string, std::string> kvs);

            void set_path(std::vector<std::string> path);

            bool validate();

        private:
            std::shared_ptr<message> m_msg;
        };
    }
}




#endif

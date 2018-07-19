
#pragma once

#include<string>
#include<vector>


#define NODE_ID_VERSION                          '0'
#define PRIVATE_KEY_VERSION                 '0'


namespace matrix
{
    namespace core
    {

        struct node_info
        {
            std::string node_id;

            std::string node_private_key;
        };

        class id_generator
        {
        public:
            id_generator();
            ~id_generator();

            int32_t generate_node_info(node_info &info);

            std::string generate_check_sum();

            std::string generate_session_id();

            std::string generate_nonce();

            std::string generate_task_id();

            //bool check_node_info(const std::string & node_id, const std::string & node_privarte_key);
            bool check_node_id(const std::string & node_id);
            
            bool check_node_private_key(const std::string & node_privarte_key);
            bool check_base58_id(const std::string & id);

            bool check_node_id_by_privatekey(const std::string & node_id, const std::string & node_privarte_key);
        private:
            bool decode_node_id(const std::string & node_id, std::vector<uint8_t> & vch);
            bool decode_private_key(const std::string & node_id, std::vector<uint8_t> & vch);


        };
    }
}



#pragma once

#include<string>


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

        private:

        };
    }
}


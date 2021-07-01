#pragma once

#include "common.h"
#include "peer_seeds.h"

namespace matrix
{
    namespace core
    {
        class net_type_params
        {
        public:
            net_type_params() = default;

            virtual ~net_type_params() = default;

            void init_seeds();

            const std::string & get_net_listen_port() {
                return m_net_listen_port;
            }

            void init_net_listen_port(const std::string & net_listen_port) {
                m_net_listen_port = net_listen_port;
            }

        public:
            const std::vector<const char *> &get_dns_seeds() {
                return m_dns_seeds;
            }

            const std::vector<peer_seeds> &get_hard_code_seeds() {
                return m_hard_code_seeds;
            }

        protected:
            std::string m_net_listen_port;
            std::vector<const char *> m_dns_seeds;
            std::vector<peer_seeds> m_hard_code_seeds;
        };
    }
}
#pragma once

#include "service_module.h"

namespace matrix
{
    namespace service_core
    {
        class common_service : public matrix::core::service_module
        {
        public:
            common_service();

            virtual ~common_service() = default;

            virtual std::string module_name() const { return common_service_name; }

        protected:
            virtual void init_invoker();

            virtual void init_timer();

            virtual int32_t service_init(bpo::variables_map &options);

            virtual int32_t service_exit();

            int32_t on_timer_filter_clean(std::shared_ptr<matrix::core::core_timer> timer);

        private:
            uint32_t m_timer_id_filter_clean;

        };

    }
}


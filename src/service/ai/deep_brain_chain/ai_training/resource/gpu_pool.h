
#pragma once

#include <stdint.h>
#include <set>
#include <map>
#include <string>
#include <mutex>
#include <memory>

namespace ai
{
    namespace dbc
    {

        class gpu
        {
        public:
            gpu();

            gpu(int32_t id, std::string type = "");

            int32_t id() const { return m_id; }

            std::string type() const { return m_type; }

            std::string toString();

            friend bool operator<(const gpu &lhs, const gpu &rhs);

        private:
            int32_t m_id;
            std::string m_type;
        };


        class gpu_pool
        {
        public:
            gpu_pool();

            void init(std::set<gpu> gpus);

            std::shared_ptr<gpu> allocate(int32_t gpu_id);
            bool allocate(std::string gpu_str);
            bool allocate(std::set<int32_t>&);

            bool check(std::string gpu_str);
            bool check(std::set<int32_t>&);

            void free(int32_t gpu_id);
            void free(std::set<int32_t>&);
            void free(std::string gpu_str);

            bool check();

            int32_t count();
            int32_t count_busy();
            int32_t count_free();

            std::set<int32_t> ids();

            std::string toString();

        private:
            std::mutex m_mutex;
            std::map<int32_t, std::shared_ptr<gpu>> m_gpus;
            std::set<int32_t> m_gpus_free;
            std::set<int32_t> m_gpus_busy;

        };


        class gpu_pool_helper{
        public:
            static std::set<int32_t> parse_gpu_list(gpu_pool&, std::string);
            static void parse_gpu_info(gpu_pool& pool, std::string str);
            static void update_gpu_from_proc(gpu_pool& pool, std::string);
        };

    }
}

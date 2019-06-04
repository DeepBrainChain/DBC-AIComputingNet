#include "gpu_pool.h"
#include "common/util.h"

#include <boost/format.hpp>

namespace ai
{
    namespace dbc
    {

        gpu::gpu()
        {
            m_id = -1;
            m_type = "";
        }

        gpu::gpu(int32_t id, std::string type)
        {
            m_id = id;
            m_type = type;
        }

        std::string gpu::toString()
        {
            return boost::str(boost::format("%-4s %s") %m_id %m_type );
        }


        bool operator<(const gpu &lhs, const gpu &rhs)
        {
            return lhs.id() < rhs.id();
        }


        gpu_pool::gpu_pool()
        {
        }

        void gpu_pool::init(std::set<gpu> gpus)
        {
            std::unique_lock<std::mutex> lock(m_mutex);

            m_gpus.clear();
            m_gpus_busy.clear();
            m_gpus_free.clear();

            for (auto &i: gpus)
            {
                m_gpus[i.id()] = std::make_shared<gpu>(i);
                m_gpus_free.insert(i.id());
            }
        }

        bool gpu_pool::check(std::string gpu_str)
        {
            auto ids = gpu_pool_helper::parse_gpu_list(*this, gpu_str);
            return check(ids);
        }

        bool gpu_pool::check(std::set<int32_t>& ids)
        {
            std::unique_lock<std::mutex> lock(m_mutex);

            for(auto const& id: ids)
            {
                if (m_gpus_free.find(id) == m_gpus_free.end())
                {
                    return false;
                }
            }
            return true;

        }

        bool gpu_pool::allocate(std::set<int32_t>& ids)
        {
            std::unique_lock<std::mutex> lock(m_mutex);

            for(auto const& id: ids)
            {
                if (m_gpus_free.find(id) == m_gpus_free.end())
                {
                    return false;
                }
            }


            for (auto const& id: ids)
            {
                m_gpus_busy.insert(id);
                m_gpus_free.erase(id);
            }

            return true;
        }


        bool gpu_pool::allocate(std::string gpu_str)
        {
            auto ids = gpu_pool_helper::parse_gpu_list(*this, gpu_str);
            return allocate(ids);
        }


        void gpu_pool::free(std::set<int32_t>& ids)
        {
            std::unique_lock<std::mutex> lock(m_mutex);

            for (auto const& id: ids)
            {
                if(m_gpus_busy.find(id) != m_gpus_busy.end())
                {
                    m_gpus_free.insert(id);
                    m_gpus_busy.erase(id);
                }
            }

        }

        void gpu_pool::free(std::string gpu_str)
        {
            auto ids = gpu_pool_helper::parse_gpu_list(*this, gpu_str);
            free(ids);
        }



        std::set<int32_t> gpu_pool::ids()
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            std::set<int32_t> rt;
            for (auto const& it: m_gpus)
            {
                rt.insert(it.first);
            }

            return rt;
        }

        std::shared_ptr<gpu> gpu_pool::allocate(int32_t gpu_id)
        {
            std::unique_lock<std::mutex> lock(m_mutex);

            auto it = m_gpus_free.find(gpu_id);
            if (it != m_gpus_free.end())
            {
                m_gpus_busy.insert(gpu_id);
                m_gpus_free.erase(it);
                return m_gpus[gpu_id];
            }

            return nullptr;
        }

        void gpu_pool::free(int32_t gpu_id)
        {
            std::unique_lock<std::mutex> lock(m_mutex);

            auto it = m_gpus_busy.find(gpu_id);
            if (it != m_gpus_busy.end())
            {
                m_gpus_free.insert(gpu_id);
                m_gpus_busy.erase(it);
            }
        }

        int32_t gpu_pool::count() {
            std::unique_lock<std::mutex> lock(m_mutex);
            return m_gpus.size();
        }

        int32_t gpu_pool::count_busy() {
            std::unique_lock<std::mutex> lock(m_mutex);
            return m_gpus_busy.size();
        }

        int32_t gpu_pool::count_free() {
            std::unique_lock<std::mutex> lock(m_mutex);
            return m_gpus_free.size();
        }


        bool gpu_pool::check()
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            if(m_gpus.size() != (m_gpus_free.size() + m_gpus_busy.size()))
            {
                return false;
            }

            return true;
        }


        std::string gpu_pool::toString()
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            std::string s;

            if (m_gpus.empty())
                return s;

            s = "{\"gpus\":[";

            int n = 0;
            for (auto const& it: m_gpus)
            {

                std::string state = "busy";
                if (m_gpus_free.find(it.first) != m_gpus_free.end())
                {
                    state = "idle";
                }

                if (n)
                {
                    s+=",";
                }

                std::string each = std::string("{") + "\"id\":\"" + std::to_string((int)it.second->id()) + "\",\"state\":\"" + state + "\"}";
                s += each;

                n++;
            }

            s+="]}";

            return s;
        }


        void gpu_pool_helper::parse_gpu_info(gpu_pool& pool, std::string str)
        {
            std::vector<std::string> vec;
            matrix::core::string_util::split(str, "\n", vec);

            std::set<gpu> gpus;
            int i = 0;
            for (auto &s: vec)
            {
                matrix::core::string_util::trim(s);
                if (!s.empty() && s != "Unknown")
                {
                    gpus.insert(gpu(i,s));
                    i++;
                }
            }

            pool.init(gpus);
        }

        void gpu_pool_helper::update_gpu_from_proc(gpu_pool& pool, std::string path)
        {
            #if defined(__linux__) || defined(MAC_OSX)

//            std::string path= "/proc/driver/nvidia/gpus";
            std::string cmd = "for i in `ls " + path + "`; do cat " + path + "/$i/information | grep Model | awk '{out=\"\"; for(i=2;i<=NF;i++){out=out$i}; print out}' ;done";
            FILE *proc = popen(cmd.c_str(), "r");
            if (proc != NULL)
            {
                const int LINE_SIZE = 4096;
                char line[LINE_SIZE];
                std::string result;

                while (fgets(line, LINE_SIZE, proc)) {
                    result += line;
                }
                pclose(proc);

                matrix::core::string_util::trim(result);

                gpu_pool_helper::parse_gpu_info(pool, result);
            }

            #endif
        }

        std::set<int32_t> gpu_pool_helper::parse_gpu_list(gpu_pool& pool, std::string gpu_str)
        {
            std::set<int32_t> gpu_ids;

            // gpu_str: {"all", "none", "0,1,2,3"}
            matrix::core::string_util::trim(gpu_str);
            if (gpu_str == "none")
            {
                //
            }
            else if (gpu_str == "all")
            {
                return pool.ids();
            }
            else
            {


                std::vector<std::string> gpu_str_vec;
                matrix::core::string_util::split(gpu_str, ",", gpu_str_vec);
                for (std::string s: gpu_str_vec)
                {
                    try
                    {
                        int32_t id = std::stoi(s);
                        if (id >= 0)
                        {
                            gpu_ids.insert(id);
                        }
                    }
                    catch (...)
                    {
                        //
                    }
                }
            }

            return gpu_ids;
        }

    }
}


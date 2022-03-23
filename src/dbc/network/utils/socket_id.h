#ifndef DBC_NETWORK_SOCKET_ID_H
#define DBC_NETWORK_SOCKET_ID_H

#include <atomic>
#include <boost/serialization/singleton.hpp>
#include <sstream>

using namespace boost::serialization;

namespace network
{
    enum socket_type
    {
        SERVER_SOCKET = 0,
        CLIENT_SOCKET = 1
    };

    class socket_id
    {
    public:
        socket_id() = default;

        socket_id(socket_type _type, uint64_t _id) : m_type(_type), m_id(_id) {}

        socket_id(const socket_id &sid) {
            this->m_type = sid.m_type;
            this->m_id = sid.m_id;
        }

        virtual ~socket_id() = default;

        uint64_t get_id() const { return m_id; }

        socket_type get_type() const { return m_type; }

        socket_id & operator=(const socket_id &sid) { 
            if (this != &sid) {
                this->m_id = sid.m_id;
                this->m_type = sid.m_type;
            }

            return *this; 
        }

        //bool operator==(const socket_id &sid) { return (m_id == sid.get_id()) && (m_type == sid.get_type()); }

        //bool operator!=(const socket_id &sid) { return !(*this == sid); }

        std::string to_string() const 
        { 
            std::stringstream ss;
            ss << " socket type: " << ((m_type == CLIENT_SOCKET) ? "client" : "server") << " socket id: " << m_id;            
            return  ss.str();
        }

    protected:
        socket_type m_type = SERVER_SOCKET;
        uint64_t m_id = 0;
    };

    inline bool operator==(const socket_id &s1, const socket_id &s2)
    {
        return (s1.get_id() == s2.get_id()) && (s1.get_type() == s2.get_type());
    }

    inline bool operator!=(const socket_id &s1, const socket_id &s2)
    {
        return !(s1 == s2);
    }
        
    class cmp_key
    {
    public:
        bool operator() (const socket_id &s1, const socket_id &s2) const
        {
            return (s1.get_type() < s2.get_type()) || ((s1.get_type() == s2.get_type()) && (s1.get_id() < s2.get_id()));
        }
    };

    class socket_id_allocator : public singleton<socket_id_allocator>
    {
    public:
        socket_id alloc_server_socket_id()
        {
            return socket_id(SERVER_SOCKET, ++m_cur_idx);
        }

        socket_id alloc_client_socket_id()
        {
            return socket_id(CLIENT_SOCKET, ++m_cur_idx);
        }

    protected:
        std::atomic<uint64_t> m_cur_idx {0};
    };
}

#endif

#include "byte_buf.h"

byte_buf::byte_buf(int32_t len, bool auto_alloc)
        : m_len(len)
        , m_buf(new char[len](), std::default_delete<char[]>())
        , m_write_ptr(m_buf.get())
        , m_write_bound(m_write_ptr + m_len)
        , m_auto_alloc(auto_alloc)
        , m_read_ptr(m_buf.get())
        , m_read_bound(m_read_ptr) {}

    int32_t  byte_buf::write_to_byte_buf(const char * in_buf, uint32_t buf_len)
    {
        assert(in_buf != nullptr && buf_len != 0);

        //copy data directly
        if (m_write_ptr + buf_len <= m_write_bound)
        {
            std::memcpy(m_write_ptr, in_buf, buf_len);

            //move
            m_write_ptr += buf_len;
            m_read_bound = m_write_ptr;

            return buf_len;
        }
        else
        {
            if (m_auto_alloc)
            {
                uint32_t new_len = m_len << 1;
                 while (new_len <= MAX_BYTE_BUF_LEN)
                 {
                    //enough to copy
                    if ((new_len - get_valid_read_len()) > buf_len)
                    {
                        m_len = new_len;
                        char_ptr new_buf(new char[m_len](), std::default_delete<char[]>());
                        char *new_buf_ptr = new_buf.get();

                        //move data to new buffer
                        memcpy(new_buf_ptr, m_read_ptr, get_valid_read_len());
                        m_write_ptr = new_buf_ptr + get_valid_read_len();
                        m_write_bound = new_buf_ptr + m_len;
                        m_read_bound = m_write_ptr;
                        m_read_ptr = new_buf_ptr;

                        //take over raw ptr
                        m_buf = new_buf;

                        //copy
                        std::memcpy(m_write_ptr, in_buf, buf_len);

                        //move
                        m_write_ptr += buf_len;
                        m_read_bound = m_write_ptr;

                        return buf_len;
                    }

                    new_len <<= 1;
                }
            }

            //error exception
            //LOG_ERROR << "byte buf is not enough to write, in_buf len: " << buf_len;
            throw std::length_error("read from buf but not enough");
        }
    }

    int32_t byte_buf::read_from_byte_buf(char * out_buf, uint32_t buf_len)
    {
        assert(out_buf != nullptr);

        //enough to read
        if (m_read_ptr + buf_len <= m_read_bound)
        {
            std::memcpy(out_buf, m_read_ptr, buf_len);
            m_read_ptr += buf_len;

            return buf_len;
        }
        else
        {
            //LOG_ERROR << "byte buf is not enough to read, out_buf len: " << buf_len;
            throw std::length_error("read from buf but not enough");
        }
    }

    uint32_t byte_buf::get_valid_read_len()
    {
        if (m_read_ptr < m_read_bound)
        {
            return (uint32_t)(m_read_bound - m_read_ptr);
        }
        else
        {
            return 0;
        }
    }

    uint32_t byte_buf::get_valid_write_len()
    {
        if (m_write_ptr < m_write_bound)
        {
            return (uint32_t)(m_write_bound - m_write_ptr);
        }
        else
        {
            return 0;
        }
    }

    int32_t byte_buf::move_write_ptr(uint32_t move_len)
    {
        assert(move_len > 0);

        if (move_len <= get_valid_write_len())
        {
            m_write_ptr += move_len;
            m_read_bound = m_write_ptr;
            return ERR_SUCCESS;
        }
        else
        {
            //LOG_ERROR << "byte_buf move write ptr error, move_len: " << move_len << " ,valid write len: " << get_valid_write_len();
            throw std::length_error("byte buf move write ptr err and write ptr beyond valid write length");
        }
    }

    int32_t byte_buf::move_read_ptr(uint32_t move_len)
    {
        if (move_len < get_valid_read_len())
        {
            m_read_ptr += move_len;
        }
        else if (move_len == get_valid_read_len())
        {
            reset();
        }
        else
        {
            //LOG_ERROR << "byte_buf move read ptr error, move_len: " << move_len << " ,valid read len: " << get_valid_read_len();
            throw std::length_error("byte buf move read ptr  err and read ptr beyond valid read length");
        }

        return ERR_SUCCESS;
    }

    //move to header
    void byte_buf::move_buf()
    {
        if (m_read_ptr == m_buf.get())
        {
            return;
        }
        //read over
        else if (m_read_ptr == m_write_ptr)
        {
            reset();
        }
        else if (m_read_ptr < m_write_ptr)
        {
            uint32_t move_len = (uint32_t)(m_write_ptr - m_read_ptr);
            if (move_len == m_len)
            {
                return;
            }
            else if (move_len < m_len)
            {
                //move bytes
                memmove(m_buf.get(), m_read_ptr, move_len);

                //move ptr
                m_read_ptr = m_buf.get();
                m_write_ptr = m_read_ptr + move_len;
                m_read_bound = m_write_ptr;
            }
            else
            {
                //LOG_ERROR << "byte buf move error, out_buf len too long beyond buf len, move len: " << move_len;
                throw std::length_error("byte buf move error, out_buf len too long beyond buf len");
            }
        }
        else
        {
            //LOG_ERROR << "byte buf move err and read ptr beyond write ptr";
            throw std::length_error("byte buf err and read ptr beyond write ptr");
        }
    }

    void byte_buf::reset()
    {
        //move ptr
        m_read_ptr = m_buf.get();
        m_write_ptr = m_read_ptr;
        m_read_bound = m_write_ptr;
    }

    std::string byte_buf::to_string()
    {
        int valid_read_len = get_valid_read_len();

        std::string hex_string;
        hex_string.resize(valid_read_len * 3, 0x00);

        int i = 0;
        char hex[3];
        while (i < valid_read_len)
        {
#ifdef WIN32
            sprintf_s(hex, sizeof(hex), "%02X", (uint8_t)m_read_ptr[i]);
#else
            sprintf(hex, "%02X", (uint8_t)m_read_ptr[i]);
#endif

            hex_string.at(3 * i) = hex[0];
            hex_string.at(3 * i + 1) = hex[1];
            hex_string.at(3 * i + 2) = ' ';

            i++;
        }

        return hex_string;
    }

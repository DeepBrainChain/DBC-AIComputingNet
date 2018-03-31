/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        £ºporotocol.h
* description    £ºdbc thrift style protocol codec
* date                  : 2018.01.20
* author            £ºBruce Feng
**********************************************************************************/
#pragma once


#ifdef WIN32
// Need to come before any Windows.h includes
#include <Winsock2.h>
#elif defined(__linux__) || defined(MAC_OSX)
#include <netinet/in.h>
#include <arpa/inet.h>
#endif
#include <string>
#include <vector>
#include "common.h"
#include "byte_buf.h"
#include "env_manager.h"


using namespace std;


#define MAX_STRING_LIMIT                (10 * 1024 * 1024)          //10M
#define MAX_CONTAINER_LIMIT         10240


namespace matrix
{
    namespace core
    {

        enum TType 
        {
            T_STOP = 0x7F,
            T_VOID = 1,
            T_BOOL = 2,
            T_BYTE = 3,
            T_I08 = 3,
            T_I16 = 6,
            T_I32 = 8,
            T_U64 = 9,
            T_I64 = 10,
            T_DOUBLE = 4,
            T_STRING = 11,
            T_UTF7 = 11,
            T_STRUCT = 12,
            T_MAP = 13,
            T_SET = 14,
            T_LIST = 15,
            T_UTF8 = 16,
            T_UTF16 = 17
        };

        enum TMessageType 
        {
            T_CALL = 1,
            T_REPLY = 2,
            T_EXCEPTION = 3,
            T_ONEWAY = 4
        };

        //htonll
        inline uint64_t htonll(uint64_t val)
        {
            if (env_manager::get_endian_type() == little_endian)
            {
                return (((unsigned long long)htonl((int)((val << 32) >> 32))) << 32) | (unsigned int)htonl((int)(val >> 32));
            }
            else if (env_manager::get_endian_type() == big_endian)
            {
                return val;
            }
            else
            {
                throw invalid_argument("endian type error");
            }
        }

        //ntohll
        inline uint64_t ntohll(uint64_t val)
        {
            if (env_manager::get_endian_type() == little_endian)
            {
                return (((unsigned long long)ntohl((int)((val << 32) >> 32))) << 32) | (unsigned int)ntohl((int)(val >> 32));
            }
            else if (env_manager::get_endian_type() == big_endian)
            {
                return val;
            }
            else
            {
                throw invalid_argument("endian type error");
            }
        }

        class byte_order
        {
        public:

            static uint16_t hton16(uint16_t x) { return htons(x); }
            static uint32_t hton32(uint32_t x) { return htonl(x); }
            static uint64_t hton64(uint64_t x) { return htonll(x); }
            static uint16_t ntoh16(uint16_t x) { return ntohs(x); }
            static uint32_t ntoh32(uint32_t x) { return ntohl(x); }
            static uint64_t ntoh64(uint64_t x) { return ntohll(x); }
        };

        template <typename To, typename From>
        static inline To bitwise_cast(From from) 
        {
            BOOST_STATIC_ASSERT(sizeof(From) == sizeof(To));
            union 
            {
                From f;
                To t;
            } u;

            u.f = from;
            return u.t;
        }

        template <class Protocol_>
        uint32_t skip(Protocol_& prot, TType type) 
        {
            //TInputRecursionTracker tracker(prot);

            switch (type) 
            {
            case T_BOOL: 
            {
                bool boolv;
                return prot.readBool(boolv);
            }
            case T_BYTE: 
            {
                int8_t bytev = 0;
                return prot.readByte(bytev);
            }
            case T_I16: 
            {
                int16_t i16;
                return prot.readI16(i16);
            }
            case T_I32: 
            {
                int32_t i32;
                return prot.readI32(i32);
            }
            case T_I64: 
            {
                int64_t i64;
                return prot.readI64(i64);
            }
            case T_DOUBLE: 
            {
                double dub;
                return prot.readDouble(dub);
            }
            case T_STRING: 
            {
                std::string str;
                return prot.readBinary(str);
            }
            case T_STRUCT: 
            {
                uint32_t result = 0;
                std::string name;
                int16_t fid;
                TType ftype;
                result += prot.readStructBegin(name);
                while (true) 
                {
                    result += prot.readFieldBegin(name, ftype, fid);
                    if (ftype == T_STOP) 
                    {
                        break;
                    }
                    result += skip(prot, ftype);
                    result += prot.readFieldEnd();
                }
                result += prot.readStructEnd();
                return result;
            }
            case T_MAP: 
            {
                uint32_t result = 0;
                TType keyType;
                TType valType;
                uint32_t i, size;
                result += prot.readMapBegin(keyType, valType, size);
                for (i = 0; i < size; i++) 
                {
                    result += skip(prot, keyType);
                    result += skip(prot, valType);
                }
                result += prot.readMapEnd();
                return result;
            }
            case T_SET: 
            {
                uint32_t result = 0;
                TType elemType;
                uint32_t i, size;
                result += prot.readSetBegin(elemType, size);
                for (i = 0; i < size; i++) 
                {
                    result += skip(prot, elemType);
                }
                result += prot.readSetEnd();
                return result;
            }
            case T_LIST: 
            {
                uint32_t result = 0;
                TType elemType;
                uint32_t i, size;
                result += prot.readListBegin(elemType, size);
                for (i = 0; i < size; i++) 
                {
                    result += skip(prot, elemType);
                }
                result += prot.readListEnd();
                return result;
            }
            case T_STOP:
            case T_VOID:
            case T_U64:
            case T_UTF8:
            case T_UTF16:
                break;
            default:
                throw invalid_argument("skip invalid data");
            }

            return 0;
        }

        class protocol
        {
        public:

            virtual ~protocol() = default;

            virtual void init_buf(byte_buf *buf) = 0;

            virtual uint32_t writeMessageBegin(const std::string& name,
                const TMessageType messageType,
                const int32_t seqid) = 0;

            virtual uint32_t writeMessageEnd() = 0;

            virtual uint32_t writeStructBegin(const char* name) = 0;

            virtual uint32_t writeStructEnd() = 0;

            virtual uint32_t writeFieldBegin(const char* name, const TType fieldType, const int16_t fieldId) = 0;

            virtual uint32_t writeFieldEnd() = 0;

            virtual uint32_t writeFieldStop() = 0;

            virtual uint32_t writeMapBegin(const TType keyType, const TType valType, const uint32_t size) = 0;

            virtual uint32_t writeMapEnd() = 0;

            virtual uint32_t writeListBegin(const TType elemType, const uint32_t size) = 0;

            virtual uint32_t writeListEnd() = 0;

            virtual uint32_t writeSetBegin(const TType elemType, const uint32_t size) = 0;

            virtual uint32_t writeSetEnd() = 0;

            virtual uint32_t writeBool(const bool value) = 0;

            virtual uint32_t writeByte(const int8_t byte) = 0;

            virtual uint32_t writeI16(const int16_t i16) = 0;

            virtual uint32_t writeI32(const int32_t i32) = 0;

            virtual uint32_t writeI64(const int64_t i64) = 0;

            virtual uint32_t writeDouble(const double dub) = 0;

            virtual uint32_t writeString(const std::string& str) = 0;

            virtual uint32_t writeBinary(const std::string& str) = 0;

            virtual uint32_t readMessageBegin(std::string& name, TMessageType& messageType, int32_t& seqid) = 0;

            virtual uint32_t readMessageEnd() = 0;

            virtual uint32_t readStructBegin(std::string& name) = 0;

            virtual uint32_t readStructEnd() = 0;

            virtual uint32_t readFieldBegin(std::string& name, TType& fieldType, int16_t& fieldId) = 0;

            virtual uint32_t readFieldEnd() = 0;

            virtual uint32_t readMapBegin(TType& keyType, TType& valType, uint32_t& size) = 0;

            virtual uint32_t readMapEnd() = 0;

            virtual uint32_t readListBegin(TType& elemType, uint32_t& size) = 0;

            virtual uint32_t readListEnd() = 0;

            virtual uint32_t readSetBegin(TType& elemType, uint32_t& size) = 0;

            virtual uint32_t readSetEnd() = 0;

            virtual uint32_t readBool(bool& value) = 0;

            virtual uint32_t readByte(int8_t& byte) = 0;

            virtual uint32_t readI16(int16_t& i16) = 0;

            virtual uint32_t readI32(int32_t& i32) = 0;

            virtual uint32_t readI64(int64_t& i64) = 0;

            virtual uint32_t readDouble(double& dub) = 0;

            virtual uint32_t readString(std::string& str) = 0;

            virtual uint32_t readBinary(std::string& str) = 0;

            /**
            * Method to arbitrarily skip over data.
            */
            virtual uint32_t skip(TType type) = 0;
        };

        class binary_protocol : public protocol
        {
        public:

            binary_protocol(int32_t string_limit = MAX_STRING_LIMIT, int32_t container_limit = MAX_CONTAINER_LIMIT)
                : m_buf(nullptr)
                , m_string_limit(string_limit)
                , m_container_limit(container_limit)
            {}

            binary_protocol(byte_buf *buf, int32_t string_limit = MAX_STRING_LIMIT, int32_t container_limit = MAX_CONTAINER_LIMIT)
                : m_buf(buf) 
                , m_string_limit(string_limit)
                , m_container_limit(container_limit)
            {}

            virtual ~binary_protocol() = default;

            void init_buf(byte_buf *buf) { m_buf = buf; }

            virtual uint32_t writeMessageBegin(const std::string& name,
                const TMessageType messageType,
                const int32_t seqid) 
            {
                return 0;
            }

            virtual uint32_t writeMessageEnd() { return 0; }

            virtual uint32_t writeStructBegin(const char* name) { return 0; }

            virtual uint32_t writeStructEnd() { return 0; }

            virtual uint32_t writeFieldBegin(const char* name, const TType fieldType, const int16_t fieldId)
            {
                uint32_t wsize = 0;
                wsize += writeByte((int8_t)fieldType);
                wsize += writeI16(fieldId);
                return wsize;
            }

            virtual uint32_t writeFieldEnd() { return 0; }

            virtual uint32_t writeFieldStop() { return writeByte((int8_t)T_STOP);}

            virtual uint32_t writeMapBegin(const TType keyType, const TType valType, const uint32_t size)
            {
                uint32_t wsize = 0;
                wsize += writeByte((int8_t)keyType);
                wsize += writeByte((int8_t)valType);
                wsize += writeI32((int32_t)size);
                return wsize;
            }

            virtual uint32_t writeMapEnd() { return 0; }

            virtual uint32_t writeListBegin(const TType elemType, const uint32_t size)
            {
                uint32_t wsize = 0;
                wsize += writeByte((int8_t)elemType);
                wsize += writeI32((int32_t)size);
                return wsize;
            }

            virtual uint32_t writeListEnd() { return 0; }

            virtual uint32_t writeSetBegin(const TType elemType, const uint32_t size)
            {
                uint32_t wsize = 0;
                wsize += writeByte((int8_t)elemType);
                wsize += writeI32((int32_t)size);
                return wsize;
            }

            virtual uint32_t writeSetEnd() { return 0; }

            virtual uint32_t writeBool(const bool value)
            {
                char tmp = value ? 1 : 0;
                m_buf->write_to_byte_buf(&tmp, 1);
                return 1;
            }

            virtual uint32_t writeByte(const int8_t byte)
            {
                m_buf->write_to_byte_buf((char*)&byte, 1);
                return 1;
            }

            virtual uint32_t writeI16(const int16_t i16)
            {
                int16_t net = (int16_t)byte_order::hton16(i16);
                m_buf->write_to_byte_buf((char*)&net, 2);
                return 2;
            }

            virtual uint32_t writeI32(const int32_t i32)
            {
                int32_t net = (int32_t)byte_order::hton32(i32);
                m_buf->write_to_byte_buf((char*)&net, 4);
                return 4;
            }

            virtual uint32_t writeI64(const int64_t i64)
            {
                int64_t net = (int64_t)byte_order::hton64(i64);
                m_buf->write_to_byte_buf((char*)&net, 8);
                return 8;
            }

            virtual uint32_t writeDouble(const double dub)
            {
                BOOST_STATIC_ASSERT(sizeof(double) == sizeof(uint64_t));
                BOOST_STATIC_ASSERT(std::numeric_limits<double>::is_iec559);

                uint64_t bits = bitwise_cast<uint64_t>(dub);
                bits = byte_order::hton64(bits);
                m_buf->write_to_byte_buf((char*)&bits, 8);
                return 8;
            }

            virtual uint32_t writeString(const std::string& str)
            {
                if (str.size() > static_cast<size_t>((std::numeric_limits<int32_t>::max)()))
                    throw std::length_error("write string len error: " + str.size());

                uint32_t size = static_cast<uint32_t>(str.size());
                uint32_t result = writeI32((int32_t)size);
                if (size > 0) {
                    m_buf->write_to_byte_buf((char*)str.data(), size);
                }
                return result + size;
            }

            virtual uint32_t writeBinary(const std::string& str)
            {
                return writeString(str);
            }

            virtual uint32_t readMessageBegin(std::string& name, TMessageType& messageType, int32_t& seqid)
            {
                return 0;
            }

            virtual uint32_t readMessageEnd() { return 0; }

            virtual uint32_t readStructBegin(std::string& name) { return 0; }

            virtual uint32_t readStructEnd() { return 0; }

            virtual uint32_t readFieldBegin(std::string& name, TType& fieldType, int16_t& fieldId)
            {
                uint32_t result = 0;
                int8_t type;

                result += readByte(type);
                fieldType = (TType)type;

                if (fieldType == T_STOP) 
                {
                    fieldId = 0;
                    return result;
                }

                result += readI16(fieldId);
                return result;
            }

            virtual uint32_t readFieldEnd() { return 0; }

            virtual uint32_t readMapBegin(TType& keyType, TType& valType, uint32_t& size)
            {
                int8_t k, v;
                uint32_t result = 0;
                int32_t sizei;

                result += readByte(k);
                keyType = (TType)k;

                result += readByte(v);

                valType = (TType)v;
                result += readI32(sizei);

                if (sizei < 0) 
                {
                    throw std::length_error("read map negative size: " + sizei);
                }
                else if (this->m_container_limit && sizei > this->m_container_limit) 
                {
                    throw std::length_error("read map beyond limit size" + sizei);
                }
                size = (uint32_t)sizei;
                return result;
            }

            virtual uint32_t readMapEnd() { return 0; }

            virtual uint32_t readListBegin(TType& elemType, uint32_t& size)
            {
                int8_t e;
                uint32_t result = 0;
                int32_t sizei;

                result += readByte(e);

                elemType = (TType)e;
                result += readI32(sizei);

                if (sizei < 0) 
                {
                    throw std::length_error("read list negative size: " + sizei);
                }
                else if (this->m_container_limit && sizei > this->m_container_limit)
                {
                    throw std::length_error("read list beyond limit size" + sizei);
                }
                size = (uint32_t)sizei;
                return result;
            }

            virtual uint32_t readListEnd() { return 0; }

            virtual uint32_t readSetBegin(TType& elemType, uint32_t& size)
            {
                int8_t e;
                uint32_t result = 0;
                int32_t sizei;

                result += readByte(e);

                elemType = (TType)e;
                result += readI32(sizei);

                if (sizei < 0) {
                    throw std::length_error("read set negative size: " + sizei);
                }
                else if (this->m_container_limit && sizei > this->m_container_limit)
                {
                    throw std::length_error("read set beyond limit size" + sizei);
                }
                size = (uint32_t)sizei;
                return result;
            }

            virtual uint32_t readSetEnd() { return 0; }

            virtual uint32_t readBool(bool& value)
            {
                char b[1];
                m_buf->read_from_byte_buf(b, 1);
                value = *(int8_t*)b != 0;
                return 1;
            }

            virtual uint32_t readByte(int8_t& byte)
            {
                char b[1];
                m_buf->read_from_byte_buf(b, 1);
                byte = *(int8_t*)b;
                return 1;
            }

            virtual uint32_t readI16(int16_t& i16)
            {
                union bytes {
                    char b[2];
                    int16_t all;
                } theBytes;
                m_buf->read_from_byte_buf(theBytes.b, 2);
                i16 = (int16_t)byte_order::ntoh16(theBytes.all);
                return 2;
            }

            virtual uint32_t readI32(int32_t& i32)
            {
                union bytes {
                    char b[4];
                    int32_t all;
                } theBytes;
                m_buf->read_from_byte_buf(theBytes.b, 4);
                i32 = (int32_t)byte_order::ntoh32(theBytes.all);
                return 4;
            }

            virtual uint32_t readI64(int64_t& i64)
            {
                union bytes {
                    char b[8];
                    int64_t all;
                } theBytes;
                m_buf->read_from_byte_buf(theBytes.b, 8);
                i64 = (int64_t)byte_order::ntoh64(theBytes.all);
                return 8;
            }

            virtual uint32_t readDouble(double& dub)
            {
                BOOST_STATIC_ASSERT(sizeof(double) == sizeof(uint64_t));
                BOOST_STATIC_ASSERT(std::numeric_limits<double>::is_iec559);

                union bytes {
                    char b[8];
                    uint64_t all;
                } theBytes;

                m_buf->read_from_byte_buf(theBytes.b, 8);
                theBytes.all = byte_order::ntoh64(theBytes.all);
                dub = bitwise_cast<double>(theBytes.all);
                return 8;
            }

            virtual uint32_t readString(std::string& str)
            {
                uint32_t result;
                int32_t size;
                result = readI32(size);
                return result + readStringBody(str, size);
            }

            virtual uint32_t readBinary(std::string& str)
            {
                return readString(str);
            }

            uint32_t readStringBody(std::string& str, int32_t size) 
            {
                uint32_t result = 0;

                // Catch error cases
                if (size < 0) {
                    throw std::length_error("read set negative size: " + size);
                }
                if (this->m_string_limit > 0 && size > this->m_string_limit) 
                {
                    throw std::length_error("read string beyond limit size" + size);
                }

                // Catch empty string case
                if (size == 0) {
                    str.clear();
                    return result;
                }

                str.resize(size);
                m_buf->read_from_byte_buf(reinterpret_cast<char*>(&str[0]), size);
                return (uint32_t)size;
            }

            /**
            * Method to arbitrarily skip over data.
            */
            virtual uint32_t skip(TType type)
            {
                return matrix::core::skip(*this, type);
            }

            protected:

                byte_buf * m_buf;

                int32_t m_string_limit;

                int32_t m_container_limit;
        };

        class base
        {
        public:
            virtual ~base() = default;

            virtual uint32_t validate() const { return E_SUCCESS; }
            virtual uint32_t read(protocol * iprot) { return E_SUCCESS; }
            virtual uint32_t write(protocol * oprot) { return E_SUCCESS; }
        };

    }

}
/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        porotocol.h
* description    dbc thrift style protocol codec
* date                  2018.01.20
* author            Bruce Feng
**********************************************************************************/

#include "protocol.h"
#include "TToString.h"

namespace matrix
{
    namespace core
    {

        base_header::~base_header() throw() {
        }


        void base_header::__set_magic(const int32_t val) {
            this->magic = val;
        }

        void base_header::__set_msg_name(const std::string& val) {
            this->msg_name = val;
        }

        void base_header::__set_nonce(const std::string& val) {
            this->nonce = val;
            __isset.nonce = true;
        }

        void base_header::__set_session_id(const std::string& val) {
            this->session_id = val;
            __isset.session_id = true;
        }

        void base_header::__set_path(const std::vector<std::string> & val) {
            this->path = val;
            __isset.path = true;
        }

        void base_header::__set_exten_info(const std::map<std::string, std::string> & val) {
            this->exten_info = val;
            __isset.exten_info = true;
        }
        std::ostream& operator<<(std::ostream& out, const base_header& obj)
        {
            obj.printTo(out);
            return out;
        }


        uint32_t base_header::read(::apache::thrift::protocol::TProtocol* iprot) {

            ::apache::thrift::protocol::TInputRecursionTracker tracker(*iprot);
            uint32_t xfer = 0;
            std::string fname;
            ::apache::thrift::protocol::TType ftype;
            int16_t fid;

            xfer += iprot->readStructBegin(fname);

            using ::apache::thrift::protocol::TProtocolException;

            bool isset_magic = false;
            bool isset_msg_name = false;

            while (true)
            {
                xfer += iprot->readFieldBegin(fname, ftype, fid);
                if (ftype == ::apache::thrift::protocol::T_STOP) {
                    break;
                }
                switch (fid)
                {
                case 1:
                    if (ftype == ::apache::thrift::protocol::T_I32) {
                        xfer += iprot->readI32(this->magic);
                        isset_magic = true;
                    }
                    else {
                        xfer += iprot->skip(ftype);
                    }
                    break;
                case 2:
                    if (ftype == ::apache::thrift::protocol::T_STRING) {
                        xfer += iprot->readString(this->msg_name);
                        isset_msg_name = true;
                    }
                    else {
                        xfer += iprot->skip(ftype);
                    }
                    break;
                case 3:
                    if (ftype == ::apache::thrift::protocol::T_STRING) {
                        xfer += iprot->readString(this->nonce);
                        this->__isset.nonce = true;
                    }
                    else {
                        xfer += iprot->skip(ftype);
                    }
                    break;
                case 4:
                    if (ftype == ::apache::thrift::protocol::T_STRING) {
                        xfer += iprot->readString(this->session_id);
                        this->__isset.session_id = true;
                    }
                    else {
                        xfer += iprot->skip(ftype);
                    }
                    break;
                case 5:
                    if (ftype == ::apache::thrift::protocol::T_LIST) {
                        {
                            this->path.clear();
                            uint32_t _size0;
                            ::apache::thrift::protocol::TType _etype3;
                            xfer += iprot->readListBegin(_etype3, _size0);
                            this->path.resize(_size0);
                            uint32_t _i4;
                            for (_i4 = 0; _i4 < _size0; ++_i4)
                            {
                                xfer += iprot->readString(this->path[_i4]);
                            }
                            xfer += iprot->readListEnd();
                        }
                        this->__isset.path = true;
                    } else {
                        xfer += iprot->skip(ftype);
                    }
                    break;
                case 255:
                    if (ftype == ::apache::thrift::protocol::T_MAP) {
                        {
                            this->exten_info.clear();
                            uint32_t _size0;
                            ::apache::thrift::protocol::TType _ktype1;
                            ::apache::thrift::protocol::TType _vtype2;
                            xfer += iprot->readMapBegin(_ktype1, _vtype2, _size0);
                            uint32_t _i4;
                            for (_i4 = 0; _i4 < _size0; ++_i4)
                            {
                                std::string _key5;
                                xfer += iprot->readString(_key5);
                                std::string& _val6 = this->exten_info[_key5];
                                xfer += iprot->readString(_val6);
                            }
                            xfer += iprot->readMapEnd();
                        }
                        this->__isset.exten_info = true;
                    }
                    else {
                        xfer += iprot->skip(ftype);
                    }
                    break;
                default:
                    xfer += iprot->skip(ftype);
                    break;
                }
                xfer += iprot->readFieldEnd();
            }

            xfer += iprot->readStructEnd();

            if (!isset_magic)
                throw TProtocolException(TProtocolException::INVALID_DATA);
            if (!isset_msg_name)
                throw TProtocolException(TProtocolException::INVALID_DATA);
            return xfer;
        }

        uint32_t base_header::write(::apache::thrift::protocol::TProtocol* oprot) const {
            uint32_t xfer = 0;
            ::apache::thrift::protocol::TOutputRecursionTracker tracker(*oprot);
            xfer += oprot->writeStructBegin("base_header");

            xfer += oprot->writeFieldBegin("magic", ::apache::thrift::protocol::T_I32, 1);
            xfer += oprot->writeI32(this->magic);
            xfer += oprot->writeFieldEnd();

            xfer += oprot->writeFieldBegin("msg_name", ::apache::thrift::protocol::T_STRING, 2);
            xfer += oprot->writeString(this->msg_name);
            xfer += oprot->writeFieldEnd();

            if (this->__isset.nonce) {
                xfer += oprot->writeFieldBegin("nonce", ::apache::thrift::protocol::T_STRING, 3);
                xfer += oprot->writeString(this->nonce);
                xfer += oprot->writeFieldEnd();
            }
            if (this->__isset.session_id) {
                xfer += oprot->writeFieldBegin("session_id", ::apache::thrift::protocol::T_STRING, 4);
                xfer += oprot->writeString(this->session_id);
                xfer += oprot->writeFieldEnd();
            }
            if (this->__isset.path) {
                xfer += oprot->writeFieldBegin("path", ::apache::thrift::protocol::T_LIST, 5);
                {
                    xfer += oprot->writeListBegin(::apache::thrift::protocol::T_STRING, static_cast<uint32_t>(this->path.size()));
                    std::vector<std::string> ::const_iterator _iter12;
                    for (_iter12 = this->path.begin(); _iter12 != this->path.end(); ++_iter12)
                    {
                        xfer += oprot->writeString((*_iter12));
                    }
                    xfer += oprot->writeListEnd();
                }
                xfer += oprot->writeFieldEnd();
            }
            if (this->__isset.exten_info) {
                xfer += oprot->writeFieldBegin("exten_info", ::apache::thrift::protocol::T_MAP, 255);
                {
                    xfer += oprot->writeMapBegin(::apache::thrift::protocol::T_STRING, ::apache::thrift::protocol::T_STRING, static_cast<uint32_t>(this->exten_info.size()));
                    std::map<std::string, std::string> ::const_iterator _iter7;
                    for (_iter7 = this->exten_info.begin(); _iter7 != this->exten_info.end(); ++_iter7)
                    {
                        xfer += oprot->writeString(_iter7->first);
                        xfer += oprot->writeString(_iter7->second);
                    }
                    xfer += oprot->writeMapEnd();
                }
                xfer += oprot->writeFieldEnd();
            }
            xfer += oprot->writeFieldStop();
            xfer += oprot->writeStructEnd();
            return xfer;
        }

        void swap(base_header &a, base_header &b) {
            using ::std::swap;
            swap(a.magic, b.magic);
            swap(a.msg_name, b.msg_name);
            swap(a.nonce, b.nonce);
            swap(a.session_id, b.session_id);
            swap(a.path, b.path);
            swap(a.exten_info, b.exten_info);
            swap(a.__isset, b.__isset);
        }

        base_header::base_header(const base_header& other8) {
            magic = other8.magic;
            msg_name = other8.msg_name;
            nonce = other8.nonce;
            session_id = other8.session_id;
            path = other8.path;
            exten_info = other8.exten_info;
            __isset = other8.__isset;
        }
        base_header& base_header::operator=(const base_header& other9) {
            magic = other9.magic;
            msg_name = other9.msg_name;
            nonce = other9.nonce;
            session_id = other9.session_id;
            path = other9.path;
            exten_info = other9.exten_info;
            __isset = other9.__isset;
            return *this;
        }
        void base_header::printTo(std::ostream& out) const {
            using ::apache::thrift::to_string;
            out << "base_header(";
            out << "magic=" << to_string(magic);
            out << ", " << "msg_name=" << to_string(msg_name);
            out << ", " << "nonce="; (__isset.nonce ? (out << to_string(nonce)) : (out << "<null>"));
            out << ", " << "session_id="; (__isset.session_id ? (out << to_string(session_id)) : (out << "<null>"));
            out << ", " << "path="; (__isset.path ? (out << to_string(path)) : (out << "<null>"));
            out << ", " << "exten_info="; (__isset.exten_info ? (out << to_string(exten_info)) : (out << "<null>"));
            out << ")";
        }

    }

}
#ifndef DBC_NETWORK_PROTOCOL_H
#define DBC_NETWORK_PROTOCOL_H

#include <arpa/inet.h>
#include <netinet/in.h>

#include <string>
#include <vector>

#include "config/env_manager.h"
#include "protocol_def.h"
#include "util/memory/byte_buf.h"

#define THRIFT_UNUSED_VARIABLE(x) ((void)(x))

namespace apache {
namespace thrift {
namespace protocol {
enum TType {
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
}
}  // namespace thrift
}  // namespace apache

namespace network {
using ::apache::thrift::protocol::T_BOOL;
using ::apache::thrift::protocol::T_BYTE;
using ::apache::thrift::protocol::T_DOUBLE;
using ::apache::thrift::protocol::T_I08;
using ::apache::thrift::protocol::T_I16;
using ::apache::thrift::protocol::T_I32;
using ::apache::thrift::protocol::T_I64;
using ::apache::thrift::protocol::T_LIST;
using ::apache::thrift::protocol::T_MAP;
using ::apache::thrift::protocol::T_SET;
using ::apache::thrift::protocol::T_STOP;
using ::apache::thrift::protocol::T_STRING;
using ::apache::thrift::protocol::T_STRUCT;
using ::apache::thrift::protocol::T_U64;
using ::apache::thrift::protocol::T_UTF16;
using ::apache::thrift::protocol::T_UTF7;
using ::apache::thrift::protocol::T_UTF8;
using ::apache::thrift::protocol::T_VOID;
using ::apache::thrift::protocol::TType;

enum TMessageType { T_CALL = 1, T_REPLY = 2, T_EXCEPTION = 3, T_ONEWAY = 4 };

// htonll
inline uint64_t htonll(uint64_t val) {
    if (EnvManager::instance().get_endian_type() == little_endian) {
        return (((unsigned long long)htonl((int)((val << 32) >> 32))) << 32) |
               (unsigned int)htonl((int)(val >> 32));
    } else if (EnvManager::instance().get_endian_type() == big_endian) {
        return val;
    } else {
        throw std::invalid_argument("endian type error");
    }
}

// ntohll
inline uint64_t ntohll(uint64_t val) {
    if (EnvManager::instance().get_endian_type() == little_endian) {
        return (((unsigned long long)ntohl((int)((val << 32) >> 32))) << 32) |
               (unsigned int)ntohl((int)(val >> 32));
    } else if (EnvManager::instance().get_endian_type() == big_endian) {
        return val;
    } else {
        throw std::invalid_argument("endian type error");
    }
}

class byte_order {
public:
    static uint16_t hton16(uint16_t x) { return htons(x); }
    static uint32_t hton32(uint32_t x) { return htonl(x); }
    static uint64_t hton64(uint64_t x) { return htonll(x); }
    static uint16_t ntoh16(uint16_t x) { return ntohs(x); }
    static uint32_t ntoh32(uint32_t x) { return ntohl(x); }
    static uint64_t ntoh64(uint64_t x) { return ntohll(x); }
};

template <typename To, typename From>
static inline To bitwise_cast(From from) {
    BOOST_STATIC_ASSERT(sizeof(From) == sizeof(To));
    union {
        From f;
        To t;
    } u;

    u.f = from;
    return u.t;
}

template <class Protocol_>
uint32_t skip(Protocol_& prot, TType type) {
    // TInputRecursionTracker tracker(prot);

    switch (type) {
        case T_BOOL: {
            bool boolv;
            return prot.readBool(boolv);
        }
        case T_BYTE: {
            int8_t bytev = 0;
            return prot.readByte(bytev);
        }
        case T_I16: {
            int16_t i16;
            return prot.readI16(i16);
        }
        case T_I32: {
            int32_t i32;
            return prot.readI32(i32);
        }
        case T_I64: {
            int64_t i64;
            return prot.readI64(i64);
        }
        case T_DOUBLE: {
            double dub;
            return prot.readDouble(dub);
        }
        case T_STRING: {
            std::string str;
            return prot.readBinary(str);
        }
        case T_STRUCT: {
            uint32_t result = 0;
            std::string name;
            int16_t fid;
            TType ftype;
            result += prot.readStructBegin(name);
            while (true) {
                result += prot.readFieldBegin(name, ftype, fid);
                if (ftype == T_STOP) {
                    break;
                }
                result += skip(prot, ftype);
                result += prot.readFieldEnd();
            }
            result += prot.readStructEnd();
            return result;
        }
        case T_MAP: {
            uint32_t result = 0;
            TType keyType;
            TType valType;
            uint32_t i, size;
            result += prot.readMapBegin(keyType, valType, size);
            for (i = 0; i < size; i++) {
                result += skip(prot, keyType);
                result += skip(prot, valType);
            }
            result += prot.readMapEnd();
            return result;
        }
        case T_SET: {
            uint32_t result = 0;
            TType elemType;
            uint32_t i, size;
            result += prot.readSetBegin(elemType, size);
            for (i = 0; i < size; i++) {
                result += skip(prot, elemType);
            }
            result += prot.readSetEnd();
            return result;
        }
        case T_LIST: {
            uint32_t result = 0;
            TType elemType;
            uint32_t i, size;
            result += prot.readListBegin(elemType, size);
            for (i = 0; i < size; i++) {
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
            throw std::invalid_argument("skip invalid data");
    }

    return 0;
}

class protocol {
public:
    virtual ~protocol() = default;

    virtual void init_buf(byte_buf* buf) = 0;

    virtual byte_buf* get_buf() = 0;

    virtual uint32_t writeMessageBegin(const std::string& name,
                                       const TMessageType messageType,
                                       const int32_t seqid) = 0;

    virtual uint32_t writeMessageEnd() = 0;

    virtual uint32_t writeStructBegin(const char* name) = 0;

    virtual uint32_t writeStructEnd() = 0;

    virtual uint32_t writeFieldBegin(const char* name, const TType fieldType,
                                     const int16_t fieldId) = 0;

    virtual uint32_t writeFieldEnd() = 0;

    virtual uint32_t writeFieldStop() = 0;

    virtual uint32_t writeMapBegin(const TType keyType, const TType valType,
                                   const uint32_t size) = 0;

    virtual uint32_t writeMapEnd() = 0;

    virtual uint32_t writeListBegin(const TType elemType,
                                    const uint32_t size) = 0;

    virtual uint32_t writeListEnd() = 0;

    virtual uint32_t writeSetBegin(const TType elemType,
                                   const uint32_t size) = 0;

    virtual uint32_t writeSetEnd() = 0;

    virtual uint32_t writeBool(const bool value) = 0;

    virtual uint32_t writeByte(const int8_t byte) = 0;

    virtual uint32_t writeI16(const int16_t i16) = 0;

    virtual uint32_t writeI32(const int32_t i32) = 0;

    virtual uint32_t writeI64(const int64_t i64) = 0;

    virtual uint32_t writeDouble(const double dub) = 0;

    virtual uint32_t writeString(const std::string& str) = 0;

    virtual uint32_t writeBinary(const std::string& str) = 0;

    virtual uint32_t readMessageBegin(std::string& name,
                                      TMessageType& messageType,
                                      int32_t& seqid) = 0;

    virtual uint32_t readMessageEnd() = 0;

    virtual uint32_t readStructBegin(std::string& name) = 0;

    virtual uint32_t readStructEnd() = 0;

    virtual uint32_t readFieldBegin(std::string& name, TType& fieldType,
                                    int16_t& fieldId) = 0;

    virtual uint32_t readFieldEnd() = 0;

    virtual uint32_t readMapBegin(TType& keyType, TType& valType,
                                  uint32_t& size) = 0;

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

    virtual uint32_t append_raw(const char* v, const uint32_t size) = 0;
};
}  // namespace network

namespace apache {
namespace thrift {

class TEnumIterator : public std::iterator<std::forward_iterator_tag,
                                           std::pair<int, const char*>> {
public:
    TEnumIterator(int n, int* enums, const char** names)
        : ii_(0), n_(n), enums_(enums), names_(names) {}

    int operator++() { return ++ii_; }

    bool operator!=(const TEnumIterator& end) {
        THRIFT_UNUSED_VARIABLE(end);
        assert(end.n_ == -1);
        return (ii_ != n_);
    }

    std::pair<int, const char*> operator*() const {
        return std::make_pair(enums_[ii_], names_[ii_]);
    }

private:
    int ii_;
    const int n_;
    int* enums_;
    const char** names_;
};

class TException : public std::exception {
public:
    TException() : message_() {}

    TException(const std::string& message) : message_(message) {}

    virtual ~TException() throw() {}

    virtual const char* what() const throw() {
        if (message_.empty()) {
            return "Default TException.";
        } else {
            return message_.c_str();
        }
    }

protected:
    std::string message_;
};

namespace protocol {

typedef network::protocol TProtocol;

class TInputRecursionTracker {
public:
    TInputRecursionTracker(::apache::thrift::protocol::TProtocol&) {
    }  // dummy imp for thrift auto generation code.
};

class TOutputRecursionTracker {
public:
    TOutputRecursionTracker(::apache::thrift::protocol::TProtocol&) {
    }  // dummy imp for thrift auto generation code.
};

class TProtocolException : public apache::thrift::TException {
public:
    /**
     * Error codes for the various types of exceptions.
     */
    enum TProtocolExceptionType {
        UNKNOWN = 0,
        INVALID_DATA = 1,
        NEGATIVE_SIZE = 2,
        SIZE_LIMIT = 3,
        BAD_VERSION = 4,
        NOT_IMPLEMENTED = 5,
        DEPTH_LIMIT = 6
    };

    TProtocolException() : TException(), type_(UNKNOWN) {}

    TProtocolException(TProtocolExceptionType type)
        : TException(), type_(type) {}

    TProtocolException(const std::string& message)
        : TException(message), type_(UNKNOWN) {}

    TProtocolException(TProtocolExceptionType type, const std::string& message)
        : TException(message), type_(type) {}

    virtual ~TProtocolException() throw() {}

    /**
     * Returns an error code that provides information about the type of error
     * that has occurred.
     *
     * @return Error code
     */
    TProtocolExceptionType getType() const { return type_; }

    virtual const char* what() const throw() {
        if (message_.empty()) {
            switch (type_) {
                case UNKNOWN:
                    return "TProtocolException: Unknown protocol exception";
                case INVALID_DATA:
                    return "TProtocolException: Invalid data";
                case NEGATIVE_SIZE:
                    return "TProtocolException: Negative size";
                case SIZE_LIMIT:
                    return "TProtocolException: Exceeded size limit";
                case BAD_VERSION:
                    return "TProtocolException: Invalid version";
                case NOT_IMPLEMENTED:
                    return "TProtocolException: Not implemented";
                default:
                    return "TProtocolException: (Invalid exception type)";
            }
        } else {
            return message_.c_str();
        }
    }

protected:
    /**
     * Error code
     */
    TProtocolExceptionType type_;
};

}  // namespace protocol

class TApplicationException : public TException {
public:
    /**
     * Error codes for the various types of exceptions.
     */
    enum TApplicationExceptionType {
        UNKNOWN = 0,
        UNKNOWN_METHOD = 1,
        INVALID_MESSAGE_TYPE = 2,
        WRONG_METHOD_NAME = 3,
        BAD_SEQUENCE_ID = 4,
        MISSING_RESULT = 5,
        INTERNAL_ERROR = 6,
        PROTOCOL_ERROR = 7,
        INVALID_TRANSFORM = 8,
        INVALID_PROTOCOL = 9,
        UNSUPPORTED_CLIENT_TYPE = 10
    };

    TApplicationException() : TException(), type_(UNKNOWN) {}

    TApplicationException(TApplicationExceptionType type)
        : TException(), type_(type) {}

    TApplicationException(const std::string& message)
        : TException(message), type_(UNKNOWN) {}

    TApplicationException(TApplicationExceptionType type,
                          const std::string& message)
        : TException(message), type_(type) {}

    ~TApplicationException() noexcept override = default;

    /**
     * Returns an error code that provides information about the type of error
     * that has occurred.
     *
     * @return Error code
     */
    TApplicationExceptionType getType() const { return type_; }

    const char* what() const noexcept override {
        if (message_.empty()) {
            switch (type_) {
                case UNKNOWN:
                    return "TApplicationException: Unknown application "
                           "exception";
                case UNKNOWN_METHOD:
                    return "TApplicationException: Unknown method";
                case INVALID_MESSAGE_TYPE:
                    return "TApplicationException: Invalid message type";
                case WRONG_METHOD_NAME:
                    return "TApplicationException: Wrong method name";
                case BAD_SEQUENCE_ID:
                    return "TApplicationException: Bad sequence identifier";
                case MISSING_RESULT:
                    return "TApplicationException: Missing result";
                case INTERNAL_ERROR:
                    return "TApplicationException: Internal error";
                case PROTOCOL_ERROR:
                    return "TApplicationException: Protocol error";
                case INVALID_TRANSFORM:
                    return "TApplicationException: Invalid transform";
                case INVALID_PROTOCOL:
                    return "TApplicationException: Invalid protocol";
                case UNSUPPORTED_CLIENT_TYPE:
                    return "TApplicationException: Unsupported client type";
                default:
                    return "TApplicationException: (Invalid exception type)";
            };
        } else {
            return message_.c_str();
        }
    }

    uint32_t read(apache::thrift::protocol::TProtocol* iprot) {
        uint32_t xfer = 0;
        std::string fname;
        apache::thrift::protocol::TType ftype;
        int16_t fid;

        xfer += iprot->readStructBegin(fname);

        while (true) {
            xfer += iprot->readFieldBegin(fname, ftype, fid);
            if (ftype == apache::thrift::protocol::T_STOP) {
                break;
            }
            switch (fid) {
                case 1:
                    if (ftype == apache::thrift::protocol::T_STRING) {
                        xfer += iprot->readString(message_);
                    } else {
                        xfer += iprot->skip(ftype);
                    }
                    break;
                case 2:
                    if (ftype == apache::thrift::protocol::T_I32) {
                        int32_t type;
                        xfer += iprot->readI32(type);
                        type_ = (TApplicationExceptionType)type;
                    } else {
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
        return xfer;
    }
    uint32_t write(apache::thrift::protocol::TProtocol* oprot) const {
        uint32_t xfer = 0;
        xfer += oprot->writeStructBegin("TApplicationException");
        xfer += oprot->writeFieldBegin("message",
                                       apache::thrift::protocol::T_STRING, 1);
        xfer += oprot->writeString(message_);
        xfer += oprot->writeFieldEnd();
        xfer +=
            oprot->writeFieldBegin("type", apache::thrift::protocol::T_I32, 2);
        xfer += oprot->writeI32(type_);
        xfer += oprot->writeFieldEnd();
        xfer += oprot->writeFieldStop();
        xfer += oprot->writeStructEnd();
        return xfer;
    }

protected:
    /**
     * Error code
     */
    TApplicationExceptionType type_;
};
}  // namespace thrift
}  // namespace apache

namespace network {
typedef struct _base_header__isset {
    _base_header__isset()
        : nonce(false), session_id(false), path(false), exten_info(false) {}
    bool nonce : 1;
    bool session_id : 1;
    bool path : 1;
    bool exten_info : 1;
} _base_header__isset;

class base_header {
public:
    base_header(){};
    base_header(const base_header&);
    base_header& operator=(const base_header&);
    virtual ~base_header() throw() {}

    int32_t magic = 0;
    std::string msg_name;
    std::string nonce;
    std::string session_id;
    std::vector<std::string> path;
    std::map<std::string, std::string> exten_info;

    _base_header__isset __isset;

    void __set_magic(const int32_t val);

    void __set_msg_name(const std::string& val);

    void __set_nonce(const std::string& val);

    void __set_session_id(const std::string& val);

    void __set_path(const std::vector<std::string>& val);

    void __set_exten_info(const std::map<std::string, std::string>& val);

    bool operator==(const base_header& rhs) const {
        if (!(magic == rhs.magic)) return false;
        if (!(msg_name == rhs.msg_name)) return false;
        if (__isset.nonce != rhs.__isset.nonce)
            return false;
        else if (__isset.nonce && !(nonce == rhs.nonce))
            return false;
        if (__isset.session_id != rhs.__isset.session_id)
            return false;
        else if (__isset.session_id && !(session_id == rhs.session_id))
            return false;
        if (__isset.path != rhs.__isset.path)
            return false;
        else if (__isset.path && !(path == rhs.path))
            return false;
        if (__isset.exten_info != rhs.__isset.exten_info)
            return false;
        else if (__isset.exten_info && !(exten_info == rhs.exten_info))
            return false;
        return true;
    }
    bool operator!=(const base_header& rhs) const { return !(*this == rhs); }

    bool operator<(const base_header&) const;

    uint32_t read(::apache::thrift::protocol::TProtocol* iprot);
    uint32_t write(::apache::thrift::protocol::TProtocol* oprot) const;

    virtual void printTo(std::ostream& out) const;
};

void swap(base_header& a, base_header& b);

std::ostream& operator<<(std::ostream& out, const base_header& obj);

class base {
public:
    base() = default;
    virtual ~base() = default;

    virtual uint32_t validate() const { return ERR_SUCCESS; }
    virtual uint32_t read(protocol* iprot) { return ERR_SUCCESS; }
    virtual uint32_t write(protocol* oprot) const { return ERR_SUCCESS; }
};

class msg_base : public virtual base {
public:
    msg_base() = default;
    ~msg_base() override = default;

    base_header header;
};

class msg_forward : public msg_base {
public:
    msg_forward() = default;
    ~msg_forward() override = default;

    byte_buf body;
};
}  // namespace network

namespace apache {
namespace thrift {
typedef network::base TBase;
typedef network::msg_base TMsgBase;
}  // namespace thrift
}  // namespace apache

#endif

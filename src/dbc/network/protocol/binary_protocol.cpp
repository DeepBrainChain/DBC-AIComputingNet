#include "binary_protocol.h"

// The T_STOP of enum TType in dbc is not equal to the value of thrift official
// code: ::apache::thrift::protocol::T_STOP
#define ORIGIN_T_STOP 0

namespace apache {
namespace thrift {
namespace protocol {
TBinaryProtocol::TBinaryProtocol(int32_t string_limit, int32_t container_limit)
    : m_buf(nullptr),
      m_string_limit(string_limit),
      m_container_limit(container_limit) {}
TBinaryProtocol::TBinaryProtocol(byte_buf* buf, int32_t string_limit,
                                 int32_t container_limit)
    : m_buf(buf),
      m_string_limit(string_limit),
      m_container_limit(container_limit) {}

uint32_t TBinaryProtocol::writeMessageBegin(
    const std::string& name, const network::TMessageType messageType,
    const int32_t seqid) {
    int32_t version = (VERSION_1) | ((int32_t)messageType);
    uint32_t wsize = 0;
    wsize += writeI32(version);
    wsize += writeString(name);
    wsize += writeI32(seqid);
    return wsize;
}

uint32_t TBinaryProtocol::writeMessageEnd() { return 0; }

uint32_t TBinaryProtocol::writeStructBegin(const char* name) {
    (void)name;
    return 0;
}

uint32_t TBinaryProtocol::writeStructEnd() { return 0; }

uint32_t TBinaryProtocol::writeFieldBegin(const char* name,
                                          const TType fieldType,
                                          const int16_t fieldId) {
    (void)name;
    uint32_t wsize = 0;
    wsize += writeByte((int8_t)fieldType);
    wsize += writeI16(fieldId);
    return wsize;
}

uint32_t TBinaryProtocol::writeFieldEnd() { return 0; }

uint32_t TBinaryProtocol::writeFieldStop() {
    return writeByte((int8_t)ORIGIN_T_STOP);
}

uint32_t TBinaryProtocol::writeMapBegin(const TType keyType,
                                        const TType valType,
                                        const uint32_t size) {
    uint32_t wsize = 0;
    wsize += writeByte((int8_t)keyType);
    wsize += writeByte((int8_t)valType);
    wsize += writeI32((int32_t)size);
    return wsize;
}

uint32_t TBinaryProtocol::writeMapEnd() { return 0; }

uint32_t TBinaryProtocol::writeListBegin(const TType elemType,
                                         const uint32_t size) {
    uint32_t wsize = 0;
    wsize += writeByte((int8_t)elemType);
    wsize += writeI32((int32_t)size);
    return wsize;
}

uint32_t TBinaryProtocol::writeListEnd() { return 0; }

uint32_t TBinaryProtocol::writeSetBegin(const TType elemType,
                                        const uint32_t size) {
    uint32_t wsize = 0;
    wsize += writeByte((int8_t)elemType);
    wsize += writeI32((int32_t)size);
    return wsize;
}

uint32_t TBinaryProtocol::writeSetEnd() { return 0; }

uint32_t TBinaryProtocol::writeBool(const bool value) {
    char tmp = value ? 1 : 0;
    m_buf->write_to_byte_buf(&tmp, 1);
    return 1;
}

uint32_t TBinaryProtocol::writeByte(const int8_t byte) {
    m_buf->write_to_byte_buf((char*)&byte, 1);
    return 1;
}

uint32_t TBinaryProtocol::writeI16(const int16_t i16) {
    int16_t net = (int16_t)network::byte_order::hton16(i16);
    m_buf->write_to_byte_buf((char*)&net, 2);
    return 2;
}

uint32_t TBinaryProtocol::writeI32(const int32_t i32) {
    int32_t net = (int32_t)network::byte_order::hton32(i32);
    m_buf->write_to_byte_buf((char*)&net, 4);
    return 4;
}

uint32_t TBinaryProtocol::writeI64(const int64_t i64) {
    int64_t net = (int64_t)network::byte_order::hton64(i64);
    m_buf->write_to_byte_buf((char*)&net, 8);
    return 8;
}

uint32_t TBinaryProtocol::writeDouble(const double dub) {
    BOOST_STATIC_ASSERT(sizeof(double) == sizeof(uint64_t));
    BOOST_STATIC_ASSERT(std::numeric_limits<double>::is_iec559);

    uint64_t bits = network::bitwise_cast<uint64_t>(dub);
    bits = network::byte_order::hton64(bits);
    m_buf->write_to_byte_buf((char*)&bits, 8);
    return 8;
}

uint32_t TBinaryProtocol::writeString(const std::string& str) {
    if (str.size() > static_cast<size_t>((std::numeric_limits<int32_t>::max)()))
        throw std::length_error("write string len error: " +
                                std::to_string(str.size()));

    uint32_t size = static_cast<uint32_t>(str.size());
    uint32_t result = writeI32((int32_t)size);
    if (size > 0) {
        m_buf->write_to_byte_buf((char*)str.data(), size);
    }
    return result + size;
}

uint32_t TBinaryProtocol::writeBinary(const std::string& str) {
    return writeString(str);
}

uint32_t TBinaryProtocol::readMessageBegin(std::string& name,
                                           network::TMessageType& messageType,
                                           int32_t& seqid) {
    uint32_t result = 0;
    int32_t sz;
    result += readI32(sz);

    if (sz < 0) {
        // Check for correct version number
        int32_t version = sz & VERSION_MASK;
        if (version != VERSION_1) {
            throw TProtocolException(TProtocolException::BAD_VERSION,
                                     "Bad version identifier");
        }
        messageType = (network::TMessageType)(sz & 0x000000ff);
        result += readString(name);
        result += readI32(seqid);
    } else {
        // if (this->strict_read_) {
        //     throw TProtocolException(
        //         TProtocolException::BAD_VERSION,
        //         "No version identifier... old protocol client in strict
        //         mode?");
        // } else {
        // Handle pre-versioned input
        int8_t type;
        result += readStringBody(name, sz);
        result += readByte(type);
        messageType = (network::TMessageType)type;
        result += readI32(seqid);
        // }
    }
    return result;
}

uint32_t TBinaryProtocol::readMessageEnd() { return 0; }

uint32_t TBinaryProtocol::readStructBegin(std::string& name) {
    name = "";
    return 0;
}

uint32_t TBinaryProtocol::readStructEnd() { return 0; }

uint32_t TBinaryProtocol::readFieldBegin(std::string& name, TType& fieldType,
                                         int16_t& fieldId) {
    uint32_t result = 0;
    int8_t type;

    result += readByte(type);
    fieldType = (TType)type;

    if (fieldType == ORIGIN_T_STOP) {
        fieldId = 0;
        return result;
    }

    result += readI16(fieldId);
    return result;
}

uint32_t TBinaryProtocol::readFieldEnd() { return 0; }

uint32_t TBinaryProtocol::readMapBegin(TType& keyType, TType& valType,
                                       uint32_t& size) {
    int8_t k, v;
    uint32_t result = 0;
    int32_t sizei;

    result += readByte(k);
    keyType = (TType)k;

    result += readByte(v);

    valType = (TType)v;
    result += readI32(sizei);

    if (sizei < 0) {
        throw std::length_error("read map negative size: " +
                                std::to_string(sizei));
    } else if (this->m_container_limit && sizei > this->m_container_limit) {
        throw std::length_error("read map beyond limit size" +
                                std::to_string(sizei));
    }
    size = (uint32_t)sizei;
    return result;
}

uint32_t TBinaryProtocol::readMapEnd() { return 0; }

uint32_t TBinaryProtocol::readListBegin(TType& elemType, uint32_t& size) {
    int8_t e;
    uint32_t result = 0;
    int32_t sizei;

    result += readByte(e);

    elemType = (TType)e;
    result += readI32(sizei);

    if (sizei < 0) {
        throw std::length_error("read list negative size: " +
                                std::to_string(sizei));
    } else if (this->m_container_limit && sizei > this->m_container_limit) {
        throw std::length_error("read list beyond limit size" +
                                std::to_string(sizei));
    }
    size = (uint32_t)sizei;
    return result;
}

uint32_t TBinaryProtocol::readListEnd() { return 0; }

uint32_t TBinaryProtocol::readSetBegin(TType& elemType, uint32_t& size) {
    int8_t e;
    uint32_t result = 0;
    int32_t sizei;

    result += readByte(e);

    elemType = (TType)e;
    result += readI32(sizei);

    if (sizei < 0) {
        throw std::length_error("read set negative size: " +
                                std::to_string(sizei));
    } else if (this->m_container_limit && sizei > this->m_container_limit) {
        throw std::length_error("read set beyond limit size" +
                                std::to_string(sizei));
    }
    size = (uint32_t)sizei;
    return result;
}

uint32_t TBinaryProtocol::readSetEnd() { return 0; }

uint32_t TBinaryProtocol::readBool(bool& value) {
    char b[1];
    m_buf->read_from_byte_buf(b, 1);
    value = *(int8_t*)b != 0;
    return 1;
}

uint32_t TBinaryProtocol::readByte(int8_t& byte) {
    char b[1];
    m_buf->read_from_byte_buf(b, 1);
    byte = *(int8_t*)b;
    return 1;
}

uint32_t TBinaryProtocol::readI16(int16_t& i16) {
    union bytes {
        char b[2];
        int16_t all;
    } theBytes;
    m_buf->read_from_byte_buf(theBytes.b, 2);
    i16 = (int16_t)network::byte_order::ntoh16(theBytes.all);
    return 2;
}

uint32_t TBinaryProtocol::readI32(int32_t& i32) {
    union bytes {
        char b[4];
        int32_t all;
    } theBytes;
    m_buf->read_from_byte_buf(theBytes.b, 4);
    i32 = (int32_t)network::byte_order::ntoh32(theBytes.all);
    return 4;
}

uint32_t TBinaryProtocol::readI64(int64_t& i64) {
    union bytes {
        char b[8];
        int64_t all;
    } theBytes;
    m_buf->read_from_byte_buf(theBytes.b, 8);
    i64 = (int64_t)network::byte_order::ntoh64(theBytes.all);
    return 8;
}

uint32_t TBinaryProtocol::readDouble(double& dub) {
    BOOST_STATIC_ASSERT(sizeof(double) == sizeof(uint64_t));
    BOOST_STATIC_ASSERT(std::numeric_limits<double>::is_iec559);

    union bytes {
        char b[8];
        uint64_t all;
    } theBytes;

    m_buf->read_from_byte_buf(theBytes.b, 8);
    theBytes.all = network::byte_order::ntoh64(theBytes.all);
    dub = network::bitwise_cast<double>(theBytes.all);
    return 8;
}

uint32_t TBinaryProtocol::readString(std::string& str) {
    uint32_t result;
    int32_t size;
    result = readI32(size);
    return result + readStringBody(str, size);
}

uint32_t TBinaryProtocol::readBinary(std::string& str) {
    return readString(str);
}

uint32_t TBinaryProtocol::readStringBody(std::string& str, int32_t size) {
    uint32_t result = 0;

    // Catch error cases
    if (size < 0) {
        throw std::length_error("read set negative size: " +
                                std::to_string(size));
    }
    if (this->m_string_limit > 0 && size > this->m_string_limit) {
        //                    throw std::length_error("read string beyond limit
        //                    size" + std::to_string(size));
        throw std::length_error("read string beyond limit size");
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

uint32_t TBinaryProtocol::skip(TType type) {
    // return network::skip(*this, type);
    switch (type) {
        case T_STRUCT: {
            uint32_t result = 0;
            std::string name;
            int16_t fid;
            TType ftype;
            result += readStructBegin(name);
            while (true) {
                result += readFieldBegin(name, ftype, fid);
                if (ftype == ORIGIN_T_STOP) {
                    break;
                }
                result += skip(ftype);
                result += readFieldEnd();
            }
            result += readStructEnd();
            return result;
        }
        default:
            return network::skip(*this, type);
    }
    throw TProtocolException(TProtocolException::INVALID_DATA, "invalid TType");
}

uint32_t TBinaryProtocol::append_raw(const char* v, const uint32_t size) {
    return m_buf->write_to_byte_buf(v, size);
}
}  // namespace protocol
}  // namespace thrift
}  // namespace apache

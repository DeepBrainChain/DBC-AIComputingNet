/*
 * Note: It is only used for communication with the official thrift service, not
 * for internal communication of dbc nodes.
 */

#ifndef THRIFT_BINARY_PROTOCOL_H
#define THRIFT_BINARY_PROTOCOL_H

#include "protocol.h"

namespace apache {
namespace thrift {
namespace protocol {
class TBinaryProtocol : public network::protocol {
public:
    static const int32_t VERSION_MASK = ((int32_t)0xffff0000);
    static const int32_t VERSION_1 = ((int32_t)0x80010000);
    // VERSION_2 (0x80020000) was taken by TDenseProtocol (which has since been
    // removed)

    TBinaryProtocol(int32_t string_limit = MAX_STRING_LIMIT,
                    int32_t container_limit = MAX_CONTAINER_LIMIT);
    TBinaryProtocol(byte_buf* buf, int32_t string_limit = MAX_STRING_LIMIT,
                    int32_t container_limit = MAX_CONTAINER_LIMIT);
    virtual ~TBinaryProtocol() = default;

    void init_buf(byte_buf* buf) { m_buf = buf; }

    byte_buf* get_buf() { return m_buf; }

    uint32_t writeMessageBegin(const std::string& name,
                               const network::TMessageType messageType,
                               const int32_t seqid) override;

    uint32_t writeMessageEnd() override;

    uint32_t writeStructBegin(const char* name) override;

    uint32_t writeStructEnd() override;

    virtual uint32_t writeFieldBegin(const char* name, const TType fieldType,
                                     const int16_t fieldId) override;

    virtual uint32_t writeFieldEnd() override;

    virtual uint32_t writeFieldStop() override;

    virtual uint32_t writeMapBegin(const TType keyType, const TType valType,
                                   const uint32_t size) override;

    virtual uint32_t writeMapEnd() override;

    virtual uint32_t writeListBegin(const TType elemType,
                                    const uint32_t size) override;

    virtual uint32_t writeListEnd() override;

    virtual uint32_t writeSetBegin(const TType elemType,
                                   const uint32_t size) override;

    virtual uint32_t writeSetEnd() override;

    virtual uint32_t writeBool(const bool value) override;

    virtual uint32_t writeByte(const int8_t byte) override;

    virtual uint32_t writeI16(const int16_t i16) override;

    virtual uint32_t writeI32(const int32_t i32) override;

    virtual uint32_t writeI64(const int64_t i64) override;

    virtual uint32_t writeDouble(const double dub) override;

    virtual uint32_t writeString(const std::string& str) override;

    virtual uint32_t writeBinary(const std::string& str) override;

    virtual uint32_t readMessageBegin(std::string& name,
                                      network::TMessageType& messageType,
                                      int32_t& seqid) override;

    virtual uint32_t readMessageEnd() override;

    virtual uint32_t readStructBegin(std::string& name) override;

    virtual uint32_t readStructEnd() override;

    virtual uint32_t readFieldBegin(std::string& name, TType& fieldType,
                                    int16_t& fieldId) override;

    virtual uint32_t readFieldEnd() override;

    virtual uint32_t readMapBegin(TType& keyType, TType& valType,
                                  uint32_t& size) override;

    virtual uint32_t readMapEnd() override;

    virtual uint32_t readListBegin(TType& elemType, uint32_t& size) override;

    virtual uint32_t readListEnd() override;

    virtual uint32_t readSetBegin(TType& elemType, uint32_t& size) override;

    virtual uint32_t readSetEnd() override;

    virtual uint32_t readBool(bool& value) override;

    virtual uint32_t readByte(int8_t& byte) override;

    virtual uint32_t readI16(int16_t& i16) override;

    virtual uint32_t readI32(int32_t& i32) override;

    virtual uint32_t readI64(int64_t& i64) override;

    virtual uint32_t readDouble(double& dub) override;

    virtual uint32_t readString(std::string& str) override;

    virtual uint32_t readBinary(std::string& str) override;

    /**
     * Method to arbitrarily skip over data.
     */
    virtual uint32_t skip(TType type) override;

    virtual uint32_t append_raw(const char* v, const uint32_t size) override;

protected:
    uint32_t readStringBody(std::string& str, int32_t sz);

protected:
    byte_buf* m_buf;
    int32_t m_string_limit;
    int32_t m_container_limit;
};
}  // namespace protocol
}  // namespace thrift
}  // namespace apache
#endif  // THRIFT_BINARY_PROTOCOL_H

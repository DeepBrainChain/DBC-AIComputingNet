#include "cloud_cybercafe_client.h"

#include "Preset.h"
#include "config/conf_manager.h"
#include "log/log.h"
#include "message/preset_types.h"
#include "network/protocol/binary_protocol.h"
#include "task/bare_metal/bare_metal_node_manager.h"

namespace occ {
template <typename ThriftStruct>
std::string ThriftToString(const ThriftStruct& ts) {
    using namespace apache::thrift::protocol;

    std::shared_ptr<byte_buf> buffer = std::make_shared<byte_buf>();
    TBinaryProtocol protocol(buffer.get());
    ts.write(&protocol);

    std::string ret(buffer->get_read_ptr(), buffer->get_valid_read_len());
    return std::move(ret);
}

template <typename ThriftStruct>
bool StringToThrift(const std::string& buff, ThriftStruct& ts) {
    using namespace apache::thrift::protocol;
    try {
        std::shared_ptr<byte_buf> buffer = std::make_shared<byte_buf>();
        buffer->write_to_byte_buf(buff.data(), buff.size());
        TBinaryProtocol protocol(buffer.get());
        ts.read(&protocol);
    } catch (TProtocolException& io) {
        printf("read invalid data\n");
        return false;
    }
    return true;
}

bool getSmyooDeviceInfo(occ::PresetClient& client, const std::string& host,
                        occ::SmyooDeviceInfo& info, occ::ResultStruct& rs) {
    occ::Message getDeviceInfoMsg;
    getDeviceInfoMsg.__set_version(0x01000001);
    getDeviceInfoMsg.__set_type(occ::MessageType::GET_SMYOO_DEVICE_INFO);
    getDeviceInfoMsg.__set_body("");
    getDeviceInfoMsg.__set_host(host);
    client.handleMessage(rs, getDeviceInfoMsg);
    if (rs.code == 0) {
        bool ret = StringToThrift(rs.message, info);
        if (!ret) {
            rs.code = 1;
            rs.message = "deserialize message failed";
        }
        return ret;
    }
    return false;
}

void setSmyooDevicePower(occ::PresetClient& client, const std::string& host,
                         int32_t power, occ::ResultStruct& rs) {
    occ::SmyooDevicePowerData devicePowerData;
    devicePowerData.__set_status(power);
    occ::Message setDevicePowerMsg;
    setDevicePowerMsg.__set_version(0x01000001);
    setDevicePowerMsg.__set_type(occ::MessageType::SET_SMYOO_DEVICE_POWER);
    setDevicePowerMsg.__set_body(ThriftToString(devicePowerData));
    setDevicePowerMsg.__set_host(host);
    client.handleMessage(rs, setDevicePowerMsg);
}
}  // namespace occ

CloudCybercafeClient::CloudCybercafeClient() {
    //
}

CloudCybercafeClient::~CloudCybercafeClient() {
    //
}

FResult CloudCybercafeClient::Init() {
    std::string server = ConfManager::instance().GetCloudCybercafeServer();
    std::vector<std::string> vecSplit = util::split(server, ":");
    if (vecSplit.size() != 2)
        return FResult(ERR_ERROR, "split cloud cybercafe server config error");

    m_host = vecSplit[0];
    m_port = atol(vecSplit[1].c_str());

    m_io_service_pool = std::make_shared<network::io_service_pool>();
    if (ERR_SUCCESS != m_io_service_pool->init(1))
        return FResult(ERR_ERROR, "init io service pool failed");

    if (ERR_SUCCESS != m_io_service_pool->start())
        return FResult(ERR_ERROR, "start io service pool failed");

    std::shared_ptr<apache::thrift::protocol::TBinaryProtocol> protocol =
        std::make_shared<apache::thrift::protocol::TBinaryProtocol>();
    occ::PresetClient client(*m_io_service_pool->get_io_service().get(),
                             protocol);
    bool connected = client.connect(m_host, m_port);
    if (!connected)
        return FResult(ERR_ERROR, "connect cloud cybercafe server failed");

    std::string pingRes;
    client.ping(pingRes);
    if (pingRes != "pong")
        return FResult(ERR_ERROR, "ping cloud cybercafe server error");
    return FResultOk;
}

void CloudCybercafeClient::Exit() {
    if (m_io_service_pool) {
        m_io_service_pool->stop();
        m_io_service_pool->exit();
    }
    LOG_INFO << "CloudCybercafeClient exited";
}

FResult CloudCybercafeClient::PowerControl(const std::string& node_id,
                                           const std::string& command) {
    if (command == "reset")
        return FResult(ERR_ERROR, "smyoo device not support");
    std::shared_ptr<dbc::db_bare_metal> bm =
        BareMetalNodeManager::instance().getBareMetalNode(node_id);
    if (!bm) return FResult(ERR_ERROR, "node id not existed");

    std::shared_ptr<apache::thrift::protocol::TBinaryProtocol> protocol =
        std::make_shared<apache::thrift::protocol::TBinaryProtocol>();
    occ::PresetClient client(*m_io_service_pool->get_io_service().get(),
                             protocol);
    bool connected = client.connect(m_host, m_port);
    if (!connected)
        return FResult(ERR_ERROR, "connect cloud cybercafe server failed");

    occ::ResultStruct rs;
    occ::SmyooDeviceInfo sdi;
    bool getStatus =
        occ::getSmyooDeviceInfo(client, bm->ipmi_hostname, sdi, rs);
    LOG_INFO << "host: " << bm->ipmi_hostname
             << " get smyoo device info return: " << getStatus
             << ", mcuname: " << sdi.mcuname << ", note: " << sdi.note
             << ", isonline: " << sdi.isonline << ", power: " << sdi.power
             << ", mcuid: " << sdi.mcuid;

    if (command == "status") {
        if (!getStatus)
            return FResult(ERR_ERROR, rs.message);
        else if (sdi.power == 1)
            return FResult(ERR_SUCCESS, "Power is on");
        else if (sdi.power == 0)
            return FResult(ERR_SUCCESS, "Power is off");
        else
            return FResult(ERR_ERROR, "unknown situation");
    }

    if (command != "on" && command != "off")
        return FResult(ERR_ERROR, "unsupported command");

    int32_t power = (command == "off" ? 0 : 1);
    if (getStatus && sdi.power == power) return FResultOk;

    occ::setSmyooDevicePower(client, bm->ipmi_hostname, power, rs);
    LOG_INFO << "host: " << bm->ipmi_hostname
             << " set power control: " << command << " with status: " << power
             << " return ResultStruct{'code':" << rs.code << ",'message':\""
             << rs.message << "\"}";
    return FResult(rs.code, rs.message);
}

FResult CloudCybercafeClient::SetBootDeviceOrder(const std::string& node_id,
                                                 const std::string& device) {
    return FResult(ERR_ERROR, "not support for now");
}

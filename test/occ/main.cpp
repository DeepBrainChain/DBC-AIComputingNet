#include <boost/asio.hpp>
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <iostream>

#include "Preset.h"
#include "message/preset_types.h"
// #include "network/protocol/protocol.h"
#include "network/protocol/binary_protocol.h"

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

namespace {
using namespace std;
ostream& operator<<(ostream& out, const std::vector<std::string>& vec) {
    out << "[";
    int count = 0;
    for (const auto& item : vec) {
        if (count++ > 0) out << ",";
        out << "\"" << item << "\"";
    }
    out << "]";
    return out;
}
}  // namespace

int main(int argc, char* argv[]) {
    std::string host;
    uint32_t port;

    boost::program_options::options_description opts(
        "occ test command options");
    // clang-format off
    opts.add_options()
        ("host",
            boost::program_options::value<std::string>()->default_value("192.168.1.159"),
            "host ip of occ server")
        ("port", boost::program_options::value<uint32_t>()->default_value(9090),
        "port of occ server")
        ("help", "command options help");
    // clang-format on
    try {
        boost::program_options::variables_map vm;
        boost::program_options::store(
            boost::program_options::parse_command_line(argc, argv, opts), vm);
        boost::program_options::notify(vm);

        if (vm.count("host")) {
            host = vm["host"].as<std::string>();
        }
        if (vm.count("port")) {
            port = vm["port"].as<uint32_t>();
        }
        if (vm.count("help") > 0) {
            std::cout << opts;
            return 0;
        }
    } catch (const std::exception& e) {
        std::cout << "invalid command option " << e.what() << std::endl;
        std::cout << opts;
        return 0;
    }
    std::cout << "server:" << host << ":" << port << std::endl;

    boost::asio::io_context io_context;
    try {
        std::shared_ptr<apache::thrift::protocol::TBinaryProtocol> protocol =
            std::make_shared<apache::thrift::protocol::TBinaryProtocol>();
        occ::PresetClient client(io_context, protocol);
        bool connected = client.connect(host, port);
        if (!connected) {
            std::cout << "connect failed" << std::endl;
            return 0;
        }

        // ping
        std::string pingRes;
        client.ping(pingRes);
        std::cout << "ping:" << pingRes << std::endl;

        // get boot menu
        occ::ResultStruct rs;
        occ::Message getBootMenuMsg;
        getBootMenuMsg.__set_version(0x01000001);
        getBootMenuMsg.__set_type(occ::MessageType::GET_BOOT_MENU);
        getBootMenuMsg.__set_body("");
        client.handleMessage(rs, getBootMenuMsg);
        occ::BootMenuList bml;
        if (rs.code == 0) {
            StringToThrift(rs.message, bml);
            std::cout << "getBootMenuMsg return: " << bml << std::endl;
        } else {
            std::cout << "getBootMenuMsg return ResultStruct{" << rs.code
                      << ", " << rs.message << "}" << std::endl;
        }

        // get smyoo device info
        occ::Message getDeviceInfoMsg;
        getDeviceInfoMsg.__set_version(0x01000001);
        getDeviceInfoMsg.__set_type(occ::MessageType::GET_SMYOO_DEVICE_INFO);
        getDeviceInfoMsg.__set_body("");
        getDeviceInfoMsg.__set_host("asus");
        client.handleMessage(rs, getDeviceInfoMsg);
        occ::SmyooDeviceInfo sdi;
        if (rs.code == 0) {
            StringToThrift(rs.message, sdi);
            std::cout << "getDeviceInfoMsg return: " << sdi << std::endl;
        } else {
            std::cout << "getDeviceInfoMsg return ResultStruct{" << rs.code
                      << ", " << rs.message << "}" << std::endl;
        }

        // set smyoo device power
        occ::SmyooDevicePowerData devicePowerData;
        devicePowerData.__set_status(1);
        occ::Message setDevicePowerMsg;
        setDevicePowerMsg.__set_version(0x01000001);
        setDevicePowerMsg.__set_type(occ::MessageType::SET_SMYOO_DEVICE_POWER);
        setDevicePowerMsg.__set_body(ThriftToString(devicePowerData));
        setDevicePowerMsg.__set_host("asus");
        client.handleMessage(rs, setDevicePowerMsg);
        std::cout << "setDevicePowerMsg return ResultStruct{" << rs.code << ", "
                  << rs.message << "}" << std::endl;
    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }
    return 0;
}

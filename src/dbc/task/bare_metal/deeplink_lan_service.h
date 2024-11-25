#ifndef DBC_DEEPLINK_LAN_SERVICE_H
#define DBC_DEEPLINK_LAN_SERVICE_H

#include "common/common.h"
#include "deeplink_room.h"
#include "network/utils/io_service_pool.h"
#include "service_module/service_module.h"
#include "util/utils.h"

class deeplink_lan_service : public service_module,
                             public Singleton<deeplink_lan_service> {
public:
    deeplink_lan_service() = default;

    ~deeplink_lan_service() = default;

    ERRCODE init() override;

    void exit() override;

    FResult get_device_info(const std::string& host, std::string& device_id,
                            std::string& password);
    FResult set_device_info(const std::string& host,
                            const std::string& device_id,
                            const std::string& password);

    void init_timer() override{};

    void init_invoker() override{};

private:
    void do_accept();

    std::shared_ptr<network::io_service_pool> m_io_service_pool = nullptr;
    std::shared_ptr<boost::asio::ip::tcp::acceptor> m_acceptor = nullptr;
    deeplink_room m_room;
};

#endif  // DBC_DEEPLINK_LAN_SERVICE_H

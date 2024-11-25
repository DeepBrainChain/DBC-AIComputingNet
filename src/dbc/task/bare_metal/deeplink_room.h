#ifndef DBC_DEEPLINK_ROOM_H
#define DBC_DEEPLINK_ROOM_H

#include <boost/asio.hpp>

#include "common/error.h"
#include "deeplink_participant.h"

class deeplink_room {
public:
    void join(const boost::asio::ip::tcp::endpoint& endpoint,
              std::shared_ptr<deeplink_participant> participant);
    void leave(const boost::asio::ip::tcp::endpoint& endpoint);

    // std::shared_ptr<deeplink_participant> get_participant(
    //     const std::string& host);
    FResult get_device_info(const std::string& host, std::string& device_id,
                            std::string& password);
    FResult set_device_info(const std::string& host,
                            const std::string& device_id,
                            const std::string& password);

private:
    // std::set<std::shared_ptr<chat_session>> participants_;
    std::map<boost::asio::ip::tcp::endpoint,
             std::shared_ptr<deeplink_participant>>
        participants_;
};

#endif  // DBC_DEEPLINK_ROOM_H

#include "deeplink_room.h"

#include "log/log.h"

using boost::asio::ip::tcp;

void deeplink_room::join(const tcp::endpoint& endpoint,
                         std::shared_ptr<deeplink_participant> participant) {
    LOG_INFO << "deeplink from " << endpoint << " join";
    // auto iter = participants_.find(endpoint);
    typedef std::pair<tcp::endpoint, std::shared_ptr<deeplink_participant>>
        MapType;
    auto iter =
        std::find_if(participants_.begin(), participants_.end(),
                     [&endpoint](const MapType& it) {
                         return it.first.address() == endpoint.address();
                     });
    if (iter != participants_.end()) {
        iter->second->close();
    }
    participants_[endpoint] = participant;
}

void deeplink_room::leave(const tcp::endpoint& endpoint) {
    LOG_INFO << "deeplink from " << endpoint << " leave";
    participants_.erase(endpoint);
}

// std::shared_ptr<deeplink_participant> deeplink_room::get_participant(
//     const std::string& host) {
//     typedef std::pair<tcp::endpoint, std::shared_ptr<deeplink_participant>>
//         MapType;
//     auto iter =
//         std::find_if(participants_.begin(), participants_.end(),
//                      [&endpoint](const MapType& it) {
//                          return it.first.address() == endpoint.address();
//                      });
//     if (iter != participants_.end()) {
//         return iter->second;
//     }
//     return nullptr;
// }

FResult deeplink_room::get_device_info(const std::string& host,
                                       std::string& device_id,
                                       std::string& password) {
    typedef std::pair<tcp::endpoint, std::shared_ptr<deeplink_participant>>
        MapType;
    auto iter = std::find_if(participants_.begin(), participants_.end(),
                             [&host](const MapType& it) {
                                 return it.first.address().to_string() == host;
                             });
    if (iter != participants_.end()) {
        iter->second->get_device_info(device_id, password);
        return FResultOk;
    }
    return FResult(ERR_ERROR, "deeplink service not connected");
}

FResult deeplink_room::set_device_info(const std::string& host,
                                       const std::string& device_id,
                                       const std::string& password) {
    typedef std::pair<tcp::endpoint, std::shared_ptr<deeplink_participant>>
        MapType;
    auto iter = std::find_if(participants_.begin(), participants_.end(),
                             [&host](const MapType& it) {
                                 return it.first.address().to_string() == host;
                             });
    if (iter != participants_.end()) {
        iter->second->set_device_info(device_id, password);
        return FResultOk;
    }
    return FResult(ERR_ERROR, "deeplink service not connected");
}

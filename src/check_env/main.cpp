//#include "check_websocket.h"
#include "check_kvm.h"
#include "check_images.h"
#include "util/SystemResourceManager.h"
#include "../dbc/common/version.h"
#include "../dbc/common/common.h"

int main(int argc, char** argv) {
    std::cout << "version: " << dbcversion() << std::endl;

    // "wss://infotest.dbcwallet.io"
    // "wss://galaxytest.congtu.cloud"
    // "wss://127.0.0.1:9898"
    // check_websocket::test_websocket("wss://galaxytest.congtu.cloud", "123");

    SystemResourceManager::instance().Init();

    // check_images::test_images();

    check_kvm::test_kvm(argc, argv);

    return 0;
}

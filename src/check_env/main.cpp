#include "check_websocket.h"
#include "check_kvm.h"
#include "util/SystemResourceManager.h"

int main(int argc, char** argv) {
    // "wss://infotest.dbcwallet.io"
    // "wss://galaxytest.congtu.cloud"
    // "wss://127.0.0.1:9898"
    check_websocket::test_websocket("wss://galaxytest.congtu.cloud", "123");

    SystemResourceManager::instance().Init();

    check_kvm::test_kvm(argc, argv);

    return 0;
}

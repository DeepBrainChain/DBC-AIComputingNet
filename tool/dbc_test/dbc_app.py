from manager.p2p_service_manager import *
from manager.connect_manager import *
from manager.dispatcher_manager import  *
from manager.ai_training_request_manager import *
from common_def.global_def import *

def init_service(svr_manager):
    svr_manager.service_init()
    svr_manager.start()
    g_service_list.append(svr_manager)
    g_service_map[svr_manager.srevice_name()] = svr_manager

def stop_service():
    for svr_manager in g_service_list:
        svr_manager.join()

def main():
    init_service(dispatcher_manager())
    init_service(p2p_service_manager())
    init_service(connect_manager())
    init_service(ai_training_req_manager())

    stop_service()

if __name__ == '__main__':
    main()
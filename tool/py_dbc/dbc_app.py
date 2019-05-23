from manager.p2p_service_manager import *
from manager.connect_manager import *
from manager.dispatcher_manager import  *
from manager.ai_training_request_manager import *
from manager.ai_mining_device_manager import *
from common_def.global_def import *
from dbc_message.node_ver_req import *
import time
import logging
import os.path
import web_service.dbc_web
def init_service(svr_manager):
    svr_manager.service_init()
    svr_manager.start()
    g_service_list.append(svr_manager)
    g_service_map[svr_manager.srevice_name()] = svr_manager

def stop_service():
    for svr_manager in g_service_list:
        svr_manager.join()

def init_log():
    get_logger().setLevel(logging.INFO)
    rq = time.strftime('%Y%m%d%H%M', time.localtime(time.time()))
    log_path = os.path.dirname(os.getcwd()) + '\\Logs\\'
    if not os.path.exists(log_path):
        os.mkdir(log_path, "0755")
    log_name = log_path + rq + '.log'
    logfile = log_name
    fh = logging.FileHandler(logfile, mode='w')
    fh.setLevel(logging.DEBUG)
    formatter = logging.Formatter("%(asctime)s - %(filename)s[line:%(lineno)d] - %(levelname)s: %(message)s")
    fh.setFormatter(formatter)
    get_logger().addHandler(fh)

def main():
    set_magic("E1D1A099")
    gen_node_id()
    init_log()
    get_logger().info("python dbc client start")
    init_service(dispatcher_manager())
    init_service(ai_training_req_manager())
    init_service(ai_mining_device_manager())
    init_service(connect_manager())
    init_service(p2p_service_manager())

    web_service.dbc_web.app.run()

    stop_service()

if __name__ == '__main__':
    main()
from common_def.service_name import *
import logging
g_service_map = {}
g_service_list = []

g_logger = logging.getLogger()

def get_connect_manager():
    return g_service_map[connection_manager_name]
def get_manager(mangager_name):
    return g_service_map[mangager_name]
def get_logger():
    return g_logger
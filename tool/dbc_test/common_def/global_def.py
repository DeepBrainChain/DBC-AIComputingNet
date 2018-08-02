import service_name
g_service_map = {}
g_service_list = []

def get_connect_manager():
    return g_service_map[service_name.connection_manager_name]
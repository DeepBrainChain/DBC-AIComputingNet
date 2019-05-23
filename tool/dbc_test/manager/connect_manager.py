from service_manager import *
from network.tcp_client import *
from common_def import service_name
import threading
class connect_manager(service_manager):
    def __init__(self):
        service_manager.__init__(self)

    def connect_node(self, address):
        connector =tcp_client(address)
        ret = connector.connect()

    def add_channel(self,node):
        self.dbc_node_list.append(node)

    def connect(self, address):
        node = tcp_client(address)
        return node.connect()

    def broad_cast(self, msg):
        for node in self.dbc_node_list:
            node.out_data(msg)
    def srevice_name(self):
        return service_name.connection_manager_name
    def run(self):
        while True:
            print(self.srevice_name(), threading.currentThread().ident, ctime())
            sleep(1)

    def thread_socket(self):
        for node in self.dbc_node_list:
            if node.state() != 1:
                continue
            node.process()
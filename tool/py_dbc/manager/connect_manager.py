from manager.service_manager import *
from network.tcp_client import *
from common_def import service_name
from common_def.service_name import *
from common_def.node_info import *
import threading
from time import ctime
from common_def.global_def import *
from common_def.net_type import *
from network.tcp_channel import *
from manager.service_manager import *
class connect_manager(service_manager):
    def __init__(self):
        service_manager.__init__(self)
        self.dbc_node_maps = {}
        self.mutex = threading.Lock()
    def service_init(self):
        Thread(target=self.thread_socket).start()
        Thread(target=self.thread_process_message).start()

    def connect_node(self, address):
        dbc_channel =tcp_channel(address)
        ret = dbc_channel.connect()
        if ret==0:
            self.add_channel(dbc_channel)

    def add_channel(self,dbc_channel):
        # self.mutex.acquire()
        self.dbc_node_maps[dbc_channel.get_sid()] = dbc_channel
        # self.mutex.release()

    def rm_channel(self, sid):
        # self.mutex.acquire()
        self.dbc_node_maps.pop(sid)
        # self.mutex.release()

    def broad_cast(self, msg):
        # self.mutex.acquire()
        for channel in self.dbc_node_maps.values():
            channel.send(msg)
        # self.mutex.release()

    def send_message(self, msg, sid):
        # self.mutex.acquire()
        channel = self.dbc_node_maps[sid]
        if  not channel is None == True:
            channel.send(msg)
        # self.mutex.release()

    def srevice_name(self):
        return service_name.connection_manager_name

    def run(self):
        pass

    def get_channel(self, sid):
        return self.dbc_node_maps[sid]

    def thread_socket(self):
        while True:
            # self.mutex.acquire()
            for channel in self.dbc_node_maps.values():
                if channel.state() != 1:
                    continue
                channel.process()
            # self.mutex.release()

    def thread_process_message(self):
        while True:
            # self.mutex.acquire()
            for channel in self.dbc_node_maps.values():
                if channel.state() != 1:
                    continue
                channel.deal_message()
            # self.mutex.release()
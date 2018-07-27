from network.tcp_client import *
from service_manager import *
from common_def import service_name
import threading
from common_def.global_def import *
from common_def.net_type import *
from dns import resolver
from common_def.node_candidate import  *
class p2p_service_manager(service_manager):
    def __init__(self):
        service_manager.__init__(self)
        self.candidates = []

    def service_init(self):
        self.init_timer()

    def init_timer(self):
        timer = threading.Timer(60, self.on_timer_check_network)
        timer.start()

    def on_timer_check_network(self):
        if self.candidates.__len__() < 1:
            self.add_dns_seeds()
        if self.candidates.__len__() < 1:
            self.add_hard_code_seeds()

        for candidate in self.candidates:
            get_connect_manager().connect(candidate.end_point)
        timer = threading.Timer(60, self.on_timer_check_network)
        timer.start()

    def add_dns_seeds(self):
        if g_dns.__len__() < 1 :
            return -1
        dns = g_dns.pop()
        dns_answer = resolver.query(dns, 'A')
        for address_items in dns_answer.response.answer:
            for address in address_items:
                candidate = node_candidate
                candidate.end_point = (address, 21107)

    def add_hard_code_seeds(self):
        if g_seeds_v4.__len__() <  1:
            return -1
        for seed in g_seeds_v4:
            candidate = node_candidate
            candidate.end_point = seed
    def connect_node(self, address):
        connector =dbc_tcp_client(address)
        ret = connector.connect()

    def on_ver_resp(self,msg):
        pass
    def on_get_nodes_resp(self, msg):
        pass
    def srevice_name(self):
        return service_name.p2p_service_name

    def run(self):
        while 1:
            # self.process()
            print(self.srevice_name(), threading.currentThread().ident, ctime())
            sleep(1)

    def process(self):
        print(self.srevice_name(), threading.currentThread().ident, ctime())
        # while True:
        #     print("haha")

from network.tcp_client import *
from common_def import service_name
import threading
import time
from time import *
from time import ctime
from common_def.global_def import *
from common_def.net_type import *
from dns import resolver
from common_def.node_candidate import  *
from common_def.service_name import *
from dbc_message.msg_common import *
from dbc_message.node_ver_req import *
from common_def.node_info import *
from manager.service_manager import *
class p2p_service_manager(service_manager):
    def __init__(self):
        service_manager.__init__(self)
        self.m_candidates = []
        self.m_peer_actives={}

    def service_init(self):
        self.init_timer()
        self.subscribe_init()
        self.invoker_init()

    def subscribe_init(self):
        get_manager(topic_manager_name).subcribe(CLIENT_CONNECT_NOTIFICATION, self.send)
        get_manager(topic_manager_name).subcribe(VER_RESP, self.send)
        get_manager(topic_manager_name).subcribe(P2P_GET_PEER_NODES_RESP, self.send)

    def invoker_init(self):
        self.m_invokers[CLIENT_CONNECT_NOTIFICATION]=self.on_client_connect_notification
        self.m_invokers[VER_RESP] = self.on_ver_resp
        self.m_invokers[P2P_GET_PEER_NODES_RESP] = self.on_get_peer_nodes_resp

    def init_timer(self):
        timer = threading.Timer(6, self.on_timer_check_network)
        timer.start()

    def on_client_connect_notification(self, message):
        sid = message.header.src_id
        body = message.msg_content
        if body.status != 0:
            candidate = self.find_candidate_by_sid(sid)
            candidate.net_state = "failed"
            self.rm_from_p2p_actives(sid)
            get_manager(connection_manager_name).rm_channel(sid)
            return -1
        addr = body.addr
        msg = make_ver_req(addr[0], addr[1],get_node_id())
        get_manager(connection_manager_name).send_message(msg, sid)

    def rm_from_p2p_actives(self, sid):
        for node_id, node_info in self.m_peer_actives.items():
            if node_info.m_sid == sid:
                self.m_peer_actives.pop(node_id)

    def on_ver_resp(self, message):
        content = message.msg_content
        start_time= int(time())
        # dbc_node_info = node_info(message.msg_content.node_id, message.msg_content.core_version, message.header.src_id, start_time)
        # self.m_peer_actives[message.msg_content.node_id] = dbc_node_info

        channel = get_manager(connection_manager_name).get_channel(message.header.src_id)
        if channel is None:
            return -1
        candidate = self.find_candidate(channel.get_address())
        dbc_node_info = node_info(message.msg_content.node_id, message.msg_content.core_version, message.header.src_id,
                                  start_time,channel.get_address())
        self.m_peer_actives[message.msg_content.node_id] = dbc_node_info
        if candidate is not None:
            candidate.net_state = "active"
            candidate.node_id = message.msg_content.node_id

    def on_get_peer_nodes_resp(self, message):
        body = message.msg_content
        for peer_node_info in body.peer_nodes_list:
            addr = peer_node_info.addr
            peer_node_id = peer_node_info.peer_node_id
            c_version = peer_node_info.core_version
            p_version = peer_node_info.protocol_version
            end_point=(addr.ip, addr.port)
            candidate = self.find_candidate(end_point)
            if candidate is None:
                n_candidate = node_candidate(end_point, peer_node_id)
                self.m_candidates.append(n_candidate)

    def get_p2p_actives(self):
        return self.m_peer_actives.values()

    def get_p2p_candidates(self):
        return self.m_candidates

    def on_timer_check_network(self):
        if self.m_candidates.__len__() < 1:
            self.add_dns_seeds()
        if self.m_candidates.__len__() < 1:
            self.add_hard_code_seeds()
        # print("candiates:",self.m_candidates)
        # for candidate in self.m_candidates:
        candidate = self.m_candidates.pop()
        if self.m_peer_actives.__len__() < 8:
            if candidate.net_state=="idle" or candidate.net_state=="failed":
                print("candiate:",candidate.end_point)
                candidate.net_state = "try"
                self.connect_node(candidate.end_point)
        self.m_candidates.append(candidate)
        timer = threading.Timer(60, self.on_timer_check_network)
        timer.start()

    def add_dns_seeds(self):
        if g_dns.__len__() < 1 :
            return -1
        dns = g_dns.pop()
        try:
            dns_answer = resolver.query(dns, 'A')
        except BaseException:
            print("Error: msg body decode failure")
            return
        for address_items in dns_answer.response.answer:
            for addr in address_items:
                end_point = (str(addr), 21107)
                candidate = node_candidate(end_point,"N/A","idle")
                print("add to candidates:", candidate.end_point)
                self.m_candidates.append(candidate)
        g_dns.append(dns)

    def find_candidate_by_sid(self, sid):
        channel = get_manager(connection_manager_name).get_channel(sid)
        if channel is None:
            return None
        candidate = self.find_candidate(channel.get_address())
        return candidate
    def find_candidate(self,addr):
        for candaidate in self.m_candidates:
            if candaidate.end_point == addr:
                return candaidate
        return None

    def add_hard_code_seeds(self):
        if g_seeds_v4.__len__() <  1:
            return -1
        for seed in g_seeds_v4:
            candidate = node_candidate(seed)
            self.m_candidates.append(candidate)

    def connect_node(self, address):
        conn_manager = get_manager(connection_manager_name)
        conn_manager.connect_node(address)

    def on_get_nodes_resp(self, msg):
        pass

    def srevice_name(self):
        return service_name.p2p_service_name

    # def run(self):
    #     while 1:
    #         # self.process()
    #         print(self.srevice_name(), threading.currentThread().ident, ctime())
    #         sleep(1)
    #
    # def process(self):
    #     print(self.srevice_name(), threading.currentThread().ident, ctime())
    #     # while True:
    #     #     print("haha")

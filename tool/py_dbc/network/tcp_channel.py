from  network.tcp_client import *
from matrix.net_message import *
# from queue import Queue
from Queue import Queue
from matrix.ttypes import *
from matrix.ttypes_header import *
from matrix.dbc_service import *
from thrift.protocol.TBinaryProtocol import TBinaryProtocol
from thrift.transport.TTransport import TMemoryBuffer
import threading
from dbc_message.msg_common import *
from dbc_message.inner_message import *
from dbc_message.client_tcp_connect_notification import *
from common_def.global_def import *
from common_def.service_name import *
from pprint import pprint
class tcp_channel:
    def __init__ (self, address):
        self.m_address =  address
        self.dbc_client = tcp_client(address, self.recv)
        self.m_recvpool = Queue(1000)
        self.sid = get_random_id()
        self.net_msg = net_message()
        self.service_list=set(["ver_resp","shake_hand_resp","stop_training_req","list_training_req","list_training_resp","logs_req","logs_resp","start_training_req","start_training_resp","get_peer_nodes_resp","show_req","show_resp","service_broadcast_req"])
        self.rm_state=False

    def get_sid(self):
        return self.sid

    def get_address(self):
        return self.m_address

    def connect(self):
        ret = self.dbc_client.connect()
        if ret == 0:
            self.connect_notification(0)
        else:
            self.connect_notification(-1)
        return 0

    def set_rm_state(self,state):
        self.rm_state = state
    def get_rm_state(self):
        return self.rm_state
    def send(self,out_data):
        if self.dbc_client.put(out_data) != 0:
            self.connect_notification(-1)

    def recv(self, in_data):
        ret = self.pre_deal_data(in_data)
        if  ret != 0:
            self.connect_notification(-1)
        return ret

    def state(self):
        return self.dbc_client.state()

    def process(self):
        self.dbc_client.process()
        pass
    def deal_message(self):
        if self.m_recvpool.not_empty:
            net_msg = self.m_recvpool.get()
            msg_name, message = self.decode(net_msg.data)
            if msg_name is not None or message is not None:
                print("publish msg:", msg_name)
                get_manager(topic_manager_name).publish(msg_name, message)

    def decode_packet_header(self,p):
        # 4 bytes: len
        # 4 bytes: protocol
        return p.readI32(), p.readI32()


    def decode(self, recvdata):
        lenth = len(recvdata)
        if lenth <= 0:
            return None, None

        m = TMemoryBuffer(recvdata)
        p = TBinaryProtocol(m)

        packet_header_len, protocol_type = self.decode_packet_header(p)
        try:
            h = msg_header()
            h.read(p)
        except EOFError:
            print("Error: msg header decode failure")
            return None, None
        # print("package_len:")
        # print(packet_header_len)
        # print "msg header: "
        # pprint(vars(h), indent=4)
        msg_name = h.msg_name
        print("recv msg name:", msg_name)
        if msg_name is None:
            print("big error ", binascii.hexlify(recvdata))
            return 0

        try:
            s = h.msg_name + "_body"
            if s in globals():
                t = globals()[s]
            else:
                t = empty

            body = t()
            body.read(p)
        except EOFError:
            print("Error: msg body decode failure")
            return None, None

        # print("body: ")
        # pprint(vars(body), indent=4, width=24)

        if msg_name == "ver_resp":
            self.deal_ver_resp()
        if msg_name != "shake_hand_resp":
            print("------------------------------------------------")
            print("msg header: ")
            pprint(vars(h), indent=4)
            print("body: ")
            pprint(vars(body), indent=4, width=24)
            message = inner_message()
            message.header.msg_name = str(msg_name)
            message.header.src_id = self.get_sid()
            message.msg_head = h
            message.msg_content = body
            return msg_name, message
        return None, None


    def deal_ver_resp(self):
        timer = threading.Timer(5, self.make_shake_hand)
        timer.start()
    def connect_notification(self, status):
        message = inner_message()
        message.header.msg_name = CLIENT_CONNECT_NOTIFICATION
        message.header.src_id = self.get_sid()
        notify_message = client_tcp_connect_notification
        notify_message.addr = self.dbc_client.address
        notify_message.status = status
        message.msg_content = notify_message
        get_manager(topic_manager_name).publish(CLIENT_CONNECT_NOTIFICATION, message)


    def make_shake_hand(self):
        m = TMemoryBuffer()
        p = TBinaryProtocol(m)
        msg_name = SHAKE_HAND_REQ
        head = msg_header(get_magic(), msg_name)
        head.write(p)
        req = shake_hand_req()
        req.write(p)
        p.writeMessageEnd()
        m.flush()
        buff = pack_head(m)
        # send_s = binascii.hexlify(buff)
        # print(send_s)
        self.send(buff)
        interval = 5
        timer = threading.Timer(interval, self.make_shake_hand)
        timer.start()

    def on_shake_hand_resp(self):
        pass
    def pre_deal_data(self, in_data):
        if (len(in_data) < 1):
            return
        off_set = 0
        src_len=len(in_data)
        print("recv:", binascii.hexlify(in_data))
        while off_set <= src_len:
            if self.net_msg.lenth < 1:
                self.net_msg.lenth = struct.unpack('!L', in_data[0:4])[0]
            if self.net_msg.lenth > 102400:
                return -1

            copy_len = self.net_msg.lenth - len(self.net_msg.data)
            if copy_len==0:
                self.m_recvpool.put(self.net_msg)
                self.net_msg = net_message()
                copy_len = self.net_msg.lenth - len(self.net_msg.data)
            remain_len = src_len - off_set
            if remain_len==0:
                return 0
            copy_len = min(copy_len, remain_len)
            if copy_len > 0 :
                self.net_msg.data = self.net_msg.data  + in_data[off_set:copy_len]
                # print(self.net_msg.data)
                off_set += copy_len
        return 0
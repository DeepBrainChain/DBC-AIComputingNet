from matrix.ttypes import *
from matrix.ttypes_header import *
from matrix.dbc_service import *
from thrift.protocol.TBinaryProtocol import TBinaryProtocol
from thrift.transport.TTransport import TMemoryBuffer
from socket import *
import time
from msg_common import *
import binascii

def make_get_peer_nodes_resp(dport):
    m = TMemoryBuffer()
    p = TBinaryProtocol(m)
    msg_name = P2P_GET_PEER_NODES_RESP
    nonce = get_random_id()
    head = msg_header(magic, msg_name, nonce)
    head.write(p)
    print("nonce:%s" %(nonce))

    # self.peer_node_id = peer_node_id
    # self.core_version = core_version
    # self.protocol_version = protocol_version
    # self.live_time_stamp = live_time_stamp
    # self.addr = addr
    node_list = []

    node_info = peer_node_info()
    node_info.addr=network_address("10.10.254.198",dport)
    node_info.peer_node_id = gen_node_id()
    node_info.core_version = core_version
    node_info.protocol_version = pro_version
    node_info.live_time_stamp = int(time.time())
    node_list.append(node_info)


    req=get_peer_nodes_resp_body(node_list)
    req.write(p)
    p.writeMessageEnd()
    m.flush()
    return pack_head(m)
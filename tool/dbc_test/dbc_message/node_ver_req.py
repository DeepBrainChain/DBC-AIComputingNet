from matrix.ttypes import *
from matrix.ttypes_header import *
from matrix.dbc_service import *
from thrift.protocol.TBinaryProtocol import TBinaryProtocol
from thrift.transport.TTransport import TMemoryBuffer
import time
from msg_common import *
import binascii

def make_ver_req(peer_ip, peer_port,node_id):
    m = TMemoryBuffer()
    p = TBinaryProtocol(m)
    msg_name = VER_REQ
    nonce = get_random_id()
    head = msg_header(magic, msg_name, nonce)
    head.write(p)
    print("nonce:%s, node_id:%s" %(nonce,  node_id))
    addr_me = network_address("127.0.0.1", 21107)
    addr_you = network_address(peer_ip, peer_port)
    time_stamp = int(time.time())
    req = ver_req_body(node_id, core_version, pro_version, time_stamp, addr_me, addr_you, start_height)
    req.write(p)
    p.writeMessageEnd()
    m.flush()
    return pack_head(m)
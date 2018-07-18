from matrix.ttypes import *
from matrix.ttypes_header import *
from matrix.dbc_service import *
from thrift.protocol.TBinaryProtocol import TBinaryProtocol
from thrift.transport.TTransport import TMemoryBuffer
from socket import *
import time
from msg_common import *
import binascii

def make_broad_cast_node_info(node_id, name):
    m = TMemoryBuffer()
    p = TBinaryProtocol(m)
    msg_name = SERVICE_BROADCAST_REQ
    nonce = get_random_id()
    head = msg_header(magic, msg_name, nonce)
    head.write(p)

    node_info = node_service_info()
    node_info.name = name
    node_info.time_stamp = int(time.time())
    service_list = []
    service_list.append("ai_training")
    node_info.service_list = service_list
    kvs = {}
    kvs["gpu"] = "1 * GeForce940MX"
    kvs["state"] = "idle"
    node_info.kvs = kvs
    node_map = {}
    node_map[node_id] = node_info
    req = service_broadcast_req_body(node_map)
    req.write(p)
    p.writeMessageEnd()
    m.flush()
    return pack_head(m)
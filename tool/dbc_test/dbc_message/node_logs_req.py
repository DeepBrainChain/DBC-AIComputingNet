from matrix.ttypes import *
from matrix.ttypes_header import *
from matrix.dbc_service import *
from thrift.protocol.TBinaryProtocol import TBinaryProtocol
from thrift.transport.TTransport import TMemoryBuffer
from socket import *
import time
from msg_common import *
import binascii

def make_logs_req(node_id):
    m = TMemoryBuffer()
    p = TBinaryProtocol(m)
    msg_name = LOGS_REQ
    nonce = get_random_id()
    session_id = get_random_id()
    head = msg_header(get_magic(), msg_name, nonce, session_id)
    head.write(p)
    print("logs req.nonce:%s, node_id:%s" %(nonce,  node_id))

    # def __init__(self, task_id=None, peer_nodes_list=None, head_or_tail=None, number_of_lines=None, ):
    task_id = "2opWVieBRgGTuc23JGfCxCVA7MrzeJZfMuuXG3dFbUXwih4kNJ"
    peer_nodes_list=[]
    peer_nodes_list.append(node_id)
    req=logs_req_body(task_id, peer_nodes_list, 1, 100)
    req.write(p)
    p.writeMessageEnd()
    m.flush()
    return pack_head(m)
from matrix.ttypes import *
from matrix.ttypes_header import *
from matrix.dbc_service import *
from thrift.protocol.TBinaryProtocol import TBinaryProtocol
from thrift.transport.TTransport import TMemoryBuffer
from socket import *
import time
from dbc_message.msg_common import *
import binascii

def make_stop_training_req(in_task_id):
    m = TMemoryBuffer()
    p = TBinaryProtocol(m)
    msg_name = STOP_TRAINING_REQ
    nonce = get_random_id()
    session_id = get_random_id()
    head = msg_header(get_magic(), msg_name, nonce, session_id)

    print("stop req.nonce:%s, task_id:%s" %(nonce,  in_task_id))

    req=stop_training_req_body(in_task_id)

    message = in_task_id  + nonce
    sign_algo = "ecdsa"
    origin = get_node_id()
    exten_info = {}
    exten_info["origin_id"] = origin
    exten_info["sign_algo"] = sign_algo
    exten_info["sign"] = dbc_sign(message)
    head.exten_info = exten_info

    head.write(p)
    req.write(p)
    p.writeMessageEnd()
    m.flush()
    return pack_head(m)
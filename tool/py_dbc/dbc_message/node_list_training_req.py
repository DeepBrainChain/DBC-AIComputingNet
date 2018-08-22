from matrix.ttypes import *
from matrix.ttypes_header import *
from matrix.dbc_service import *
from thrift.protocol.TBinaryProtocol import TBinaryProtocol
from thrift.transport.TTransport import TMemoryBuffer
from socket import *
import time
from dbc_message.msg_common import *
import binascii

def make_list_training_req(in_task_id):
    m = TMemoryBuffer()
    p = TBinaryProtocol(m)
    msg_name = STOP_TRAINING_REQ
    nonce = get_random_id()
    session_id = get_random_id()
    head = msg_header(get_magic(), msg_name, nonce, session_id)
    print("list req.nonce:%s, task_id:%s" %(nonce,  in_task_id))
    req=list_training_req_body(in_task_id)
    head.write(p)
    req.write(p)
    p.writeMessageEnd()
    m.flush()
    return pack_head(m)
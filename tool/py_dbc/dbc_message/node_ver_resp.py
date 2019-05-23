from matrix.ttypes import *
from matrix.ttypes_header import *
from matrix.dbc_service import *
from thrift.protocol.TBinaryProtocol import TBinaryProtocol
from thrift.transport.TTransport import TMemoryBuffer
from socket import *
import time
from dbc_message.msg_common import *
import binascii

def make_ver_resp():
    m = TMemoryBuffer()
    p = TBinaryProtocol(m)
    msg_name = VER_RESP
    nonce = get_random_id()
    head = msg_header(get_magic(), msg_name, nonce)
    head.write(p)
    req=ver_resp_body(gen_node_id(), core_version, pro_version)
    req.write(p)
    p.writeMessageEnd()
    m.flush()
    return pack_head(m)